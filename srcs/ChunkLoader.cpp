#include "ChunkLoader.hpp"

static inline int mod_floor(int a, int b) {
	int r = a % b;
	return (r < 0) ? r + b : r;
}
static inline int floor_div(int a, int b) {
	int q = a / b, r = a % b;
	return (r && ((r < 0) != (b < 0))) ? (q - 1) : q;
}

ChunkLoader::ChunkLoader(
	int seed,
	Camera &camera,
	ThreadPool &pool,
	Chrono &chronoHelper,
	std::atomic_bool *isRunning,
	std::queue<DisplayData *>	&solidStagedDataQueue,
	std::queue<DisplayData *>	&transparentStagedDataQueue
)
:
_camera(camera),
_chronoHelper(chronoHelper),
_threadPool(pool),
_buildingDisplay(false),
_isRunning(isRunning),
_perlinGenerator(seed),
_caveGen(1000, 0.01f, 0.05f, 0.6f, 0.6f, seed),
_solidStagedDataQueue(solidStagedDataQueue),
_transparentStagedDataQueue(transparentStagedDataQueue)
{
	initData();
}

ChunkLoader::~ChunkLoader()
{
	std::lock_guard<std::mutex> lock(_chunksMutex);
	for (auto it = _chunks.begin(); it != _chunks.end(); it++)
	{
		it->second->freeSubChunks();
		delete it->second;
	}
	while (_solidStagedDataQueue.size())
	{
		auto &tmp = _solidStagedDataQueue.front();
		_solidStagedDataQueue.pop();
		delete tmp;
	}
	while (_transparentStagedDataQueue.size())
	{
		auto &tmp = _transparentStagedDataQueue.front();
		_transparentStagedDataQueue.pop();
		delete tmp;
	}
}

// --- Init methods ---
void ChunkLoader::initData()
{
	_chunksMemoryUsage = 0;
	_renderDistance = RENDER_DISTANCE;
	_currentRender = 0;
	_maxRender = 400;
	_countBudget = EXTRA_CACHE_CHUNKS;
	_modifiedCount = 0;
}

void ChunkLoader::initSpawn()
{
	// Load first chunk under the player, and pop their position on top of it
	ivec2 chunkPos = _camera.getChunkPosition(CHUNK_SIZE);
	vec3 worldPos  = _camera.getWorldPosition();
	// Build a quick, coarse mesh first for immediate feedback, refined shortly after
	Chunk* first = loadChunk(0, 0, 0, chunkPos, 8);
	if (first) {
		// Build faces immediately and push a synchronous display build so first frame has geometry
		first->sendFacesToDisplay();
		updateFillData();
		// Schedule refinement to full resolution shortly after initial display
		_threadPool.enqueue([this, chunkPos]() mutable {
			ivec2 cp = chunkPos;
			this->loadChunk(0, 0, 0, cp, RESOLUTION);
			this->scheduleDisplayUpdate();
		});
	}
	TopBlock topBlock = findTopBlockY(chunkPos, {worldPos.x, worldPos.z});
	const vec3 &camPos = _camera.getPosition();

	// Place camera: feet on ground
	_camera.setPos({camPos.x - 0.5, -(topBlock.height + 1 + EYE_HEIGHT), camPos.z - 0.5});
}

// Edits (queue if chunk not ready)
bool ChunkLoader::setBlockOrQueue(ivec2 chunkPos, ivec3 worldPos, BlockType value, bool byPlayer) {
	auto chunk = getChunk(chunkPos);
	if (!chunk) {
		std::lock_guard<std::mutex> lk(_pendingMutex);
		_pendingEdits[chunkPos].push_back({worldPos, value, byPlayer});
		return false;
	}

	if (chunk->isBuilding()) {
		std::lock_guard<std::mutex> lk(_pendingMutex);
		_pendingEdits[chunkPos].push_back({worldPos, value, byPlayer});
		return false;
	}

	return setBlock(chunkPos, worldPos, value, byPlayer);
}

// Edits (queue if chunk not ready)
void ChunkLoader::applyPendingFor(const ivec2& pos) {
	std::vector<PendingBlock> edits;
	{
		std::lock_guard<std::mutex> lk(_pendingMutex);
		auto it = _pendingEdits.find(pos);
		if (it != _pendingEdits.end()) {
			edits.swap(it->second);
			_pendingEdits.erase(it);
		}
	}
	if (edits.empty()) return;

	bool anyPlayerEdit = false;
	for (const auto& e : edits) {
		setBlock(pos, e.worldPos, e.value, e.byPlayer);
		anyPlayerEdit = anyPlayerEdit || e.byPlayer;
	}

	if (anyPlayerEdit) {
		if (auto c = getChunk(pos)) {
			if (!c->getModified()) { c->setAsModified(); ++_modifiedCount; }
			c->sendFacesToDisplay();
		}
	}
	updateFillData();
}

// Single chunk loader
Chunk *ChunkLoader::loadChunk(int x, int z, int render, ivec2 &chunkPos, int resolution)
{
	Chunk *chunk = nullptr;
	ivec2 pos = {chunkPos.x - render / 2 + x, chunkPos.y - render / 2 + z};

	// First, try to find an existing chunk quickly
	{
		std::lock_guard<std::mutex> lk(_chunksMutex);
		auto it = _chunks.find(pos);
		if (it != _chunks.end())
			chunk = it->second;
	}

	if (chunk)
	{
		if (chunk->getResolution() > resolution)
			chunk->updateResolution(resolution);
		applyPendingFor(pos);
		// Touch LRU for recently used chunk
		touchLRU(pos);
	}
	else
	{
		PerlinMap *pMap = _perlinGenerator.getPerlinMap(pos, resolution);
		Chunk *newChunk = new Chunk(pos, pMap, _caveGen, *this, _threadPool, resolution);

		bool inserted = false;
		{
			std::lock_guard<std::mutex> lk(_chunksMutex);
			auto [it, didInsert] = _chunks.emplace(pos, newChunk);
			if (didInsert)
			{
				chunk = newChunk;
				inserted = true;
			}
			else
			{
				chunk = it->second;
			}
		}

		if (!inserted)
		{
			delete newChunk;
		}
		else
		{
			// Heavy init outside the map lock so neighbors created later can find us.
			chunk->loadBlocks();
			chunk->getNeighbors();

			_chunksMemoryUsage += chunk->getMemorySize();
			// Insert into LRU as most-recent entry
			touchLRU(pos);
			++_chunksCount;
			applyPendingFor(pos);
		}
	}

	{
		std::lock_guard<std::mutex> lk(_displayedChunksMutex);
		auto [it, inserted] = _displayedChunks.emplace(pos, chunk);
		if (!inserted) it->second = chunk;
		else ++_displayedCount;
	}

	// Ensure a freshly created chunk becomes visible: build its mesh once
	// and coalesce a display update. Skip if already meshed.
	if (chunk && !chunk->isReady()) {
		chunk->sendFacesToDisplay();
		scheduleDisplayUpdate();
	}

	// Enforce chunk count budget after any load
	enforceCountBudget();
	// unloadChunk();
	return chunk;
}

// Chunk loading based on frustum view
void ChunkLoader::loadChunks(ivec2 &chunkPos) {
	if (_renderDistance <= 0)
		return;

	const int renderDistance = _renderDistance;
	const int baseResolution = RESOLUTION;
	_threshold = std::numeric_limits<int>::max();

	const int maxRadius = std::max(0, (renderDistance - 1) / 2);

	struct CandidateInfo {
		glm::ivec2 offset;
		int radius;
	};

	std::vector<CandidateInfo> candidates;
	candidates.reserve(static_cast<size_t>((maxRadius * 2) + 1) * static_cast<size_t>((maxRadius * 2) + 1));
	for (int dz = -maxRadius; dz <= maxRadius; ++dz) {
		for (int dx = -maxRadius; dx <= maxRadius; ++dx) {
			glm::ivec2 offset(dx, dz);
			candidates.push_back({offset, std::max(std::abs(dx), std::abs(dz))});
		}
	}

	std::vector<char> processed(candidates.size(), 0);
	size_t remaining = candidates.size();

	std::vector<std::future<void>> retLst;
	_currentRender = 0;
	const int updateBatch = 32;
	int batchCounter = 0;
	int maxRadiusLoaded = 0;

	auto enqueueUpdate = [&]() {
		retLst.emplace_back(_threadPool.enqueue(&ChunkLoader::updateFillData, this));
		batchCounter = 0;
	};

	const float fallbackCos = std::cos(glm::radians(70.0f));

	while (remaining > 0 && getIsRunning()) {
		Frustum localFrustum;
		bool hasFrustum = false;
		{
			std::lock_guard<std::mutex> lk(_frustumMutex);
			if (_hasCachedFrustum) {
				localFrustum = _cachedFrustum;
				hasFrustum = true;
			}
		}

		glm::vec3 camChunkLoader = _camera.getWorldPosition();
		glm::vec3 camDir = _camera.getDirection();
		glm::vec2 forward2D(camDir.x, camDir.z);
		if (glm::length(forward2D) < 1e-5f)
			forward2D = glm::vec2(0.0f, 1.0f);
		else
			forward2D = glm::normalize(forward2D);

		size_t bestIndex = std::numeric_limits<size_t>::max();
		bool bestVisible = false;
		float bestForward = -2.0f;
		float bestDistance = std::numeric_limits<float>::max();

		for (size_t i = 0; i < candidates.size(); ++i) {
			if (processed[i])
				continue;

			const CandidateInfo &cand = candidates[i];
			glm::ivec2 chunkCoord = chunkPos + cand.offset;
			glm::vec3 aabbMin(
				static_cast<float>(chunkCoord.x * CHUNK_SIZE),
				-2048.0f,
				static_cast<float>(chunkCoord.y * CHUNK_SIZE));
			glm::vec3 aabbMax = aabbMin + glm::vec3(CHUNK_SIZE, 4096.0f, CHUNK_SIZE);

			float chunkCenterX = aabbMin.x + CHUNK_SIZE * 0.5f;
			float chunkCenterZ = aabbMin.z + CHUNK_SIZE * 0.5f;
			glm::vec2 toCenter2D(chunkCenterX - camChunkLoader.x, chunkCenterZ - camChunkLoader.z);
			float len = glm::length(toCenter2D);
			float forwardDot = len > 1e-5f ? glm::dot(toCenter2D / len, forward2D) : 1.0f;
			float distance2 = toCenter2D.x * toCenter2D.x + toCenter2D.y * toCenter2D.y;

			bool visible = false;
			if (hasFrustum)
				visible = localFrustum.aabbVisible(aabbMin, aabbMax);
			else
				visible = (forwardDot >= fallbackCos);

			bool better = false;
			if (bestIndex == std::numeric_limits<size_t>::max()) {
				better = true;
			} else if (visible != bestVisible) {
				better = visible;
			} else if (visible) {
				if (distance2 != bestDistance) {
					better = distance2 < bestDistance;
				} else if (forwardDot != bestForward) {
					better = forwardDot > bestForward;
				} else {
					better = cand.radius < candidates[bestIndex].radius;
				}
			} else {
				if (forwardDot != bestForward) {
					better = forwardDot > bestForward;
				} else if (distance2 != bestDistance) {
					better = distance2 < bestDistance;
				} else {
					better = cand.radius < candidates[bestIndex].radius;
				}
			}

			if (better) {
				bestIndex = i;
				bestVisible = visible;
				bestForward = forwardDot;
				bestDistance = distance2;
			}
		}
		if (bestIndex == std::numeric_limits<size_t>::max())
			break;

		const CandidateInfo &chosen = candidates[bestIndex];
		int chunkResolution = baseResolution;
		int thresholdStep = LOD_THRESHOLD;
		while ((std::abs(chosen.offset.x) >= thresholdStep || std::abs(chosen.offset.y) >= thresholdStep) && chunkResolution < CHUNK_SIZE) {
			chunkResolution = std::min(CHUNK_SIZE, chunkResolution * 2);
			if (thresholdStep > std::numeric_limits<int>::max() / 2) {
				thresholdStep = std::numeric_limits<int>::max();
				break;
			}
			thresholdStep *= 2;
			if (thresholdStep <= 0) {
				thresholdStep = std::numeric_limits<int>::max();
				break;
			}
		}
		loadChunk(chosen.offset.x, chosen.offset.y, 0, chunkPos, chunkResolution);
		processed[bestIndex] = 1;
		--remaining;
		maxRadiusLoaded = std::max(maxRadiusLoaded, chosen.radius);
		_currentRender = std::min(renderDistance, maxRadiusLoaded * 2 + 2);

		if (++batchCounter >= updateBatch)
			enqueueUpdate();

		if (hasMoved(chunkPos))
			break;
	}

	if (batchCounter > 0)
		enqueueUpdate();

	for (auto &ret : retLst) {
		while (ret.wait_for(std::chrono::milliseconds(10)) == std::future_status::timeout) {
			if (!getIsRunning())
				break;
		}
	}
}

void ChunkLoader::unloadChunks(ivec2 &newCamChunk)
{
	// make sure queued edits + dirty meshes are up-to-date
	flushDirtyChunks();

	std::vector<ivec2> toErase;
	{
		std::lock_guard<std::mutex> lk(_displayedChunksMutex);
		for (auto &kv : _displayedChunks)
		{
			if (!getIsRunning())
				break ;
			Chunk *chunk = kv.second;
			if (!chunk) continue;
			ivec2 chunkPos = chunk->getPosition();
			if (std::abs(chunkPos.x - newCamChunk.x) > _renderDistance / 2
				|| std::abs(chunkPos.y - newCamChunk.y) > _renderDistance / 2)
			{
				toErase.push_back(kv.first);
			}
		}
		for (const auto& key : toErase)
		{
			if (!getIsRunning())
				break ;
			auto it2 = _displayedChunks.find(key);
			if (it2 != _displayedChunks.end()) {
				_displayedChunks.erase(it2);
				--_displayedCount;
			}

		}
	}
	updateFillData();
	// After display set shrinks, re-check cache pressure
	enforceCountBudget();
}

// Check if player moved
bool ChunkLoader::hasMoved(ivec2 &oldPos)
{
	// Debounce movement: only signal a cancel if we moved more than 1 chunk
	// away from the load center. This avoids restarting rings too aggressively
	// when sprinting or moving quickly.
	ivec2 camChunk = _camera.getChunkPosition(CHUNK_SIZE);
	int dx = std::abs((int)std::floor(oldPos.x) - (int)std::floor(camChunk.x));
	int dz = std::abs((int)std::floor(oldPos.y) - (int)std::floor(camChunk.y));
	return (std::max(dx, dz) > 1);
}

// Update data to be rendered
void ChunkLoader::updateFillData()
{
	// If the engine is shutting down, skip any deferred heavy work
	if (!getIsRunning()) {
		return;
	}
	// Avoid piling up heavy builds and starving the render thread
	if (_buildingDisplay.exchange(true))
		return;
	// If a staged update already exists, skip this build (we will coalesce)
	{
		std::lock_guard<std::mutex> lk(_drawDataMutex);
		if (!_solidStagedDataQueue.empty() || !_transparentStagedDataQueue.empty())
		{
			_buildingDisplay = false;
			return;
		}
	}
	DisplayData *fillData = new DisplayData();
	DisplayData *transparentData = new DisplayData();
	buildFacesToDisplay(fillData, transparentData);
	{
		std::lock_guard<std::mutex> lk(_drawDataMutex);
		_solidStagedDataQueue.emplace(fillData);
		_transparentStagedDataQueue.emplace(transparentData);
	}
	_buildingDisplay = false;
}

// Vertices, indirect buffers, ssbo building
void ChunkLoader::buildFacesToDisplay(DisplayData* fillData, DisplayData* transparentFillData) {
	// snapshot displayed chunks
	if (!getIsRunning())
		return ;
	std::vector<Chunk *> snapshot;
	{
		std::lock_guard<std::mutex> lk(_displayedChunksMutex);
		snapshot.reserve(_displayedChunks.size());
		for (auto &kv : _displayedChunks) snapshot.push_back(kv.second);
	}
	// Pre-reserve to minimize reallocations during build
	size_t totalSolidVerts = 0, totalTransVerts = 0;
	size_t totalSolidCmds  = 0, totalTransCmds  = 0;
	size_t totalSSBO       = 0;
	for (const auto& c : snapshot) {
		if (!getIsRunning())
			return ;
		totalSolidVerts += c->getVertices().size();
		totalTransVerts += c->getTransparentVertices().size();
		totalSolidCmds  += c->getIndirectData().size();
		totalTransCmds  += c->getTransparentIndirectData().size();
		totalSSBO       += c->getSSBO().size();
	}
	fillData->vertexData.reserve(fillData->vertexData.size() + totalSolidVerts);
	fillData->indirectBufferData.reserve(fillData->indirectBufferData.size() + totalSolidCmds);
	fillData->ssboData.reserve(fillData->ssboData.size() + totalSSBO);
	transparentFillData->vertexData.reserve(transparentFillData->vertexData.size() + totalTransVerts);
	transparentFillData->indirectBufferData.reserve(transparentFillData->indirectBufferData.size() + totalTransCmds);

	for (const auto& c : snapshot) {
		if (!getIsRunning())
			return ;
		auto& ssbo = c->getSSBO();

		// SOLID
		auto& solidVerts = c->getVertices();
		size_t vSizeBefore = fillData->vertexData.size();
		fillData->vertexData.insert(fillData->vertexData.end(), solidVerts.begin(), solidVerts.end());

		auto& ib = c->getIndirectData();
		size_t solidCount = ib.size();
		for (size_t i = 0; getIsRunning() && i < solidCount; ++i) {
			auto cmd = ib[i];
			cmd.baseInstance += (uint32_t)vSizeBefore;
			fillData->indirectBufferData.push_back(cmd);
		}
		fillData->ssboData.insert(fillData->ssboData.end(), ssbo.begin(), ssbo.end());

		// TRANSPARENT
		auto& transpVerts = c->getTransparentVertices();
		size_t tvBefore = transparentFillData->vertexData.size();
		transparentFillData->vertexData.insert(
			transparentFillData->vertexData.end(), transpVerts.begin(), transpVerts.end()
		);

		auto& tib = c->getTransparentIndirectData();
		size_t transpCount = tib.size();
		for (size_t i = 0; getIsRunning() &&  i < transpCount; ++i) {
			auto cmd = tib[i];
			cmd.baseInstance += (uint32_t)tvBefore;
			transparentFillData->indirectBufferData.push_back(cmd);
		}
	}
}

// Dirty chunks management
void ChunkLoader::flushDirtyChunks() {
	std::vector<ivec2> toRemesh;
	{
		std::lock_guard<std::mutex> lk(_dirtyMutex);
		if (_dirtyChunks.empty()) return;
		toRemesh.assign(_dirtyChunks.begin(), _dirtyChunks.end());
		_dirtyChunks.clear();
	}
	for (const auto& p : toRemesh) {
		if (auto c = getChunk(p)) c->sendFacesToDisplay();
	}
}

void ChunkLoader::markChunkDirty(const ivec2& pos) {
	std::lock_guard<std::mutex> lk(_dirtyMutex);
	_dirtyChunks.insert(pos);
}

// Shared data getters
Chunk *ChunkLoader::getChunk(const ivec2& pos) {
	Chunk* c = nullptr;
	{
		std::lock_guard<std::mutex> lock(_chunksMutex);
		auto it = _chunks.find(pos);
		if (it != _chunks.end()) c = it->second;
	}
	if (c) touchLRU(pos);
	return c;
}

SubChunk* ChunkLoader::getSubChunk(ivec3 &position) {
	Chunk* c = nullptr;
	{
		std::lock_guard<std::mutex> lk(_chunksMutex);
		auto it = _chunks.find(ivec2(position.x, position.z));
		if (it != _chunks.end()) c = it->second;
	}
	return c ? c->getSubChunk(position.y) : nullptr;
}

BlockType ChunkLoader::getBlock(ivec2 chunkPos, ivec3 worldPos) {
	auto chunk = getChunk(chunkPos);
	if (!chunk) return AIR;
	int subChunkIndex = static_cast<int>(floor(worldPos.y / CHUNK_SIZE));
	SubChunk *subchunk = chunk->getSubChunk(subChunkIndex);
	if (!subchunk) return AIR;
	int localX = (worldPos.x % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;
	int localY = (worldPos.y % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;
	int localZ = (worldPos.z % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;
	return subchunk->getBlock({localX, localY, localZ});
}

// Shared chunk setters
bool ChunkLoader::setBlock(ivec2 chunkPos, ivec3 worldPos, BlockType value, bool byPlayer) {
	auto chunk = getChunk(chunkPos);
	if (!chunk) return false;

	const int subY = (int)std::floor((double)worldPos.y / (double)CHUNK_SIZE);
	SubChunk* sc = chunk->getOrCreateSubChunk(subY, /*generate=*/false);
	if (!sc) return false;

	const int lx = (worldPos.x % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;
	const int ly = (worldPos.y % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;
	const int lz = (worldPos.z % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;

	sc->setBlock(lx, ly, lz, value);

	// Only player actions count as modifications for eviction protection UI
	if (byPlayer && !chunk->getModified()) { chunk->setAsModified(); ++_modifiedCount; }
	markChunkDirty(chunkPos);
	return true;
}

void ChunkLoader::setViewProj(Frustum &f)
{
	std::lock_guard<std::mutex> lk(_frustumMutex);
	_cachedFrustum = f;
	_hasCachedFrustum = true;
}

TopBlock ChunkLoader::findTopBlockY(ivec2 chunkPos, ivec2 worldPos) {
	auto chunk = getChunk(chunkPos);
	if (!chunk) return TopBlock();
	int localX = (worldPos.x % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;
	int localZ = (worldPos.y % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;
	return chunk->getTopBlock(localX, localZ);
}

TopBlock ChunkLoader::findBlockUnderPlayer(ivec2 chunkPos, ivec3 worldPos) {
	std::lock_guard<std::mutex> lock(_chunksMutex);

	auto it = _chunks.find({chunkPos.x, chunkPos.y});
	if (it == _chunks.end() || !it->second)
		return TopBlock();

	Chunk* chunk = it->second;

	const int localX = mod_floor(worldPos.x, CHUNK_SIZE);
	const int localZ = mod_floor(worldPos.z, CHUNK_SIZE);

	int startSubY   = floor_div(worldPos.y, CHUNK_SIZE);
	int startLocalY = mod_floor(worldPos.y, CHUNK_SIZE);

	return chunk->getFirstSolidBelow(localX, startLocalY, localZ, startSubY);
}

bool ChunkLoader::getIsRunning() {
	// During early construction, running mutex/flag may not be wired yet
	if (_isRunning == nullptr)
		return true;
	return *_isRunning;
}

int *ChunkLoader::getCurrentRenderPtr() { return &_currentRender; }
size_t *ChunkLoader::getMemorySizePtr() { return &_chunksMemoryUsage; }
int *ChunkLoader::getRenderDistancePtr() { return &_renderDistance; }
int *ChunkLoader::getCachedChunksCountPtr() { return &_chunksCount; }
int *ChunkLoader::getDisplayedChunksCountPtr() { return &_displayedCount; }
int *ChunkLoader::getModifiedChunksCountPtr() { return &_modifiedCount; }
NoiseGenerator &ChunkLoader::getNoiseGenerator() { return _perlinGenerator; }

void ChunkLoader::printSizes() const
{
	{
		using namespace std::chrono;
		steady_clock::time_point now;
		now = steady_clock::now();
		std::cout << "[-----------------------]" << std::endl;
		std::cout << duration_cast<seconds>(now.time_since_epoch()).count() << std::endl;
		std::cout << "PRINTING SIZES" << std::endl;
		std::cout << "_solid " << _solidStagedDataQueue.size() << std::endl;
		std::cout << "_transparentStagedDataQueue " <<  _transparentStagedDataQueue.size() << std::endl;
		std::cout << "_chunkList " <<  _chunkList.size() << std::endl;
		std::cout << "_chunks " <<  _chunks.size() << std::endl;
		std::cout << "_displayedChunks " <<  _displayedChunks.size() << std::endl;
		std::cout << "[-----------------------]" << std::endl;
	}
}

// Shared data getters
void ChunkLoader::getDisplayedChunksSnapshot(std::vector<ivec2>& out) {
	std::lock_guard<std::mutex> lk(_displayedChunksMutex);
	out.clear();
	out.reserve(_displayedChunks.size());
	for (const auto& kv : _displayedChunks) out.push_back(kv.first);
}

bool ChunkLoader::hasRenderableChunks() {
	std::lock_guard<std::mutex> lk(_displayedChunksMutex);
	for (const auto& kv : _displayedChunks) {
		Chunk* c = kv.second;
		if (!c) continue;
		if (!c->isReady()) continue;
		// Consider renderable if any indirect commands exist (solid or transparent)
		if (!c->getIndirectData().empty() || !c->getTransparentIndirectData().empty())
			return true;
	}
	return false;
}

void ChunkLoader::scheduleDisplayUpdate() {
	if (_buildingDisplay) return;
	if (!getIsRunning()) return;
	_threadPool.enqueue(&ChunkLoader::updateFillData, this);
}

// --- LRU + eviction helpers ---
void ChunkLoader::touchLRU(const ivec2& pos) {
	std::lock_guard<std::mutex> lk(_chunksListMutex);
	auto it = _lruIndex.find(pos);
	if (it != _lruIndex.end()) {
		// Move existing entry to back (most recent)
		_chunkList.splice(_chunkList.end(), _chunkList, it->second);
		it->second = std::prev(_chunkList.end());
	} else {
		_chunkList.emplace_back(pos);
		_lruIndex[pos] = std::prev(_chunkList.end());
	}
}

void ChunkLoader::enforceCountBudget() {
	// Dynamic budget: visible grid + slack + modified chunks (non-evictable)
	int renderCells = _renderDistance * _renderDistance;
	std::vector<std::pair<ivec2,int>> candidates; // pos, distance
	candidates.reserve(_chunks.size());

	ivec2 camChunk = _camera.getChunkPosition(CHUNK_SIZE);

	// Snapshot displayed set for quick checks
	std::unordered_set<ivec2, ivec2_hash> displayedSnapshot;
	{
		std::lock_guard<std::mutex> lk(_displayedChunksMutex);
		displayedSnapshot.reserve(_displayedChunks.size() * 2);
		for (auto &kv : _displayedChunks) displayedSnapshot.insert(kv.first);
	}

	int modifiedCount = 0;
	{
		std::lock_guard<std::mutex> ck(_chunksMutex);
		for (auto &kv : _chunks) {
			Chunk* c = kv.second;
			if (!c) continue;
			if (c->getModified()) {
				++modifiedCount;
				continue; // never evict modified
			}
			// Prefer to only evict non-displayed chunks
			if (displayedSnapshot.find(kv.first) != displayedSnapshot.end())
				continue;
			// Chebyshev distance on chunk grid
			int dx = std::abs(kv.first.x - camChunk.x);
			int dz = std::abs(kv.first.y - camChunk.y);
			int dist = std::max(dx, dz);
			candidates.emplace_back(kv.first, dist);
		}
	}

	const int requiredBudget = renderCells + _countBudget + modifiedCount;
	if (_chunksCount <= requiredBudget) return;

	// Evict farthest first
	std::sort(candidates.begin(), candidates.end(), [](auto &a, auto &b){ return a.second > b.second; });

	for (auto &cand : candidates) {
		if (_chunksCount <= requiredBudget) break;
		evictChunkAt(cand.first);
	}
}

bool ChunkLoader::evictChunkAt(const ivec2& candidate) {
	// Lookup the chunk
	Chunk* chunk = nullptr;
	{
		std::lock_guard<std::mutex> ck(_chunksMutex);
		auto itc = _chunks.find(candidate);
		if (itc != _chunks.end()) chunk = itc->second;
	}
	if (!chunk) return false;

	// Skip chunks being built or modified to avoid losing edits
	if (chunk->isBuilding() || chunk->getModified()) return false;

	// Also skip if displayed
	{
		std::lock_guard<std::mutex> dk(_displayedChunksMutex);
		if (_displayedChunks.find(candidate) != _displayedChunks.end())
			return false;
	}

	// Evict: remove from LRU, maps and free memory
	{
		std::lock_guard<std::mutex> lk(_chunksListMutex);
		auto it = _lruIndex.find(candidate);
		if (it != _lruIndex.end()) {
			_chunkList.erase(it->second);
			_lruIndex.erase(it);
		}
	}

	size_t freed = chunk->getMemorySize();
	chunk->unloadNeighbors();

	{
		std::lock_guard<std::mutex> pk(_pendingMutex);
		_pendingEdits.erase(candidate);
	}
	{
		std::lock_guard<std::mutex> dk(_displayedChunksMutex);
		auto it = _displayedChunks.find(candidate);
		if (it != _displayedChunks.end()) {
			_displayedChunks.erase(it);
			--_displayedCount;
		}
	}
	{
		std::lock_guard<std::mutex> ck(_chunksMutex);
		auto itc = _chunks.find(candidate);
		if (itc != _chunks.end()) {
			_chunks.erase(itc);
			--_chunksCount;
		}
	}

	chunk->freeSubChunks();
	delete chunk;
	if (_chunksMemoryUsage >= freed)
		_chunksMemoryUsage -= freed;
	else
		_chunksMemoryUsage = 0;

#if SHOW_DEBUG
	std::cout << "[ChunkCache] Evicted far chunk ("
				<< candidate.x << ", " << candidate.y << ")"
				<< ", freed ~" << freed << " bytes"
				<< ", cached=" << _chunksCount
				<< ", displayed=" << _displayedCount
				<< std::endl;
#endif
	return true;
}
