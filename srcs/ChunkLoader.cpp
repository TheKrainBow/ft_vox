#include "ChunkLoader.hpp"
#include "GenerationPriority.hpp"

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
	std::mutex &sharedDrawDataMutex,
	std::queue<DisplayData *>	&solidStagedDataQueue,
	std::queue<DisplayData *>	&transparentStagedDataQueue,
	CaveWaterSettings caveWaterSettings,
	LavaSettings lavaSettings,
	CaveGenerationSettings caveSettings
)
:
_sharedDrawDataMutex(sharedDrawDataMutex),
_camera(camera),
_chronoHelper(chronoHelper),
_threadPool(pool),
_buildingDisplay(false),
_isRunning(isRunning),
_perlinGenerator(seed),
_caveGen(seed, caveSettings),
_caveWaterGen(seed, caveWaterSettings),
_lavaGen(seed, lavaSettings),
_solidStagedDataQueue(solidStagedDataQueue),
_transparentStagedDataQueue(transparentStagedDataQueue)
{
	initData();
	_displayWorker = std::thread([this] {
		for (;;) {
			{
				std::unique_lock<std::mutex> lock(_displayWakeMutex);
				_displayWake.wait(lock,[this] { return _displayStop || _displayRequested; });
				if (_displayStop) return;
				_displayRequested = false;
			}
			updateFillData(true);
		}
	});
}

ChunkLoader::~ChunkLoader()
{
	{
		std::lock_guard<std::mutex> lock(_displayWakeMutex);
		_displayStop = true;
	}
	_displayWake.notify_one();
	_displayWorker.join();
	std::lock_guard<std::mutex> lock(_chunksMutex);
	for (auto it = _chunks.begin(); it != _chunks.end(); it++)
	{
		it->second->freeSubChunks();
		delete it->second;
	}
	{
		std::lock_guard<std::mutex> lk(_sharedDrawDataMutex);
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
	// Initialize debug snapshot mirrors
	_dbg_chunksMemoryUsage = 0;
	_dbg_renderDistance = _renderDistance.load();
	_dbg_currentRender = 0;
	_dbg_chunksCount = 0;
	_dbg_displayedCount = 0;
	_dbg_modifiedCount = 0;
}

void ChunkLoader::initSpawn()
{
	// Load first chunk under the player, and pop their position on top of it
	ivec2 chunkPos = _camera.getChunkPosition(CHUNK_SIZE);
	vec3 worldPos  = _camera.getWorldPosition();
	// Build the spawn chunk at full resolution so decorative plants
	// (grass/flowers) are present immediately on first load.
	// Using a coarse LOD here caused sparse or missing flora until
	// the chunk was later reloaded/refined.
	Chunk* first = loadChunk(0, 0, 0, chunkPos, RESOLUTION);
	if (first) {
		// Build faces immediately and push a synchronous display build so first frame has geometry
		first->sendFacesToDisplay();
		updateFillData();
		// No refinement needed: loaded at full RESOLUTION.
	}
	TopBlock topBlock = findTopBlockY(chunkPos, {worldPos.x, worldPos.z});
	const vec3 &camPos = _camera.getPosition();

	// Place camera: feet on ground
	_camera.setPos({camPos.x - 0.5, -(topBlock.height + 1 + EYE_HEIGHT), camPos.z - 0.5});
}

// Edits (queue if chunk not ready)
bool ChunkLoader::setBlockOrQueue(ivec2 chunkPos, ivec3 worldPos, BlockType value, bool byPlayer) {
	std::unique_lock<std::recursive_mutex> editLock(_blockEditMutex,std::try_to_lock);
	if (!editLock.owns_lock()) {
		{
			std::lock_guard<std::mutex> lock(_pendingMutex);
			_pendingEdits[chunkPos].push_back({worldPos,value,byPlayer});
		}
		scheduleDisplayUpdate();
		return false;
	}
	auto chunk = getChunk(chunkPos);
	if (!chunk) {
		std::lock_guard<std::mutex> lk(_pendingMutex);
		_pendingEdits[chunkPos].push_back({worldPos, value, byPlayer});
		return false;
	}

	if (chunk->isBuilding() || chunk->needsGeneration(floor_div(worldPos.y, CHUNK_SIZE))) {
		std::lock_guard<std::mutex> lk(_pendingMutex);
		_pendingEdits[chunkPos].push_back({worldPos, value, byPlayer});
		return false;
	}

	bool written = setBlock(chunkPos, worldPos, value, byPlayer);
	if (written && byPlayer) {
		markBlockDirty(worldPos, true);
		scheduleDisplayUpdate();
	}
	return written;
}

// Edits (queue if chunk not ready)
void ChunkLoader::applyPendingFor(const ivec2& pos) {
	auto* chunk = getChunk(pos);
	if (!chunk || chunk->isBuilding()) return;
	std::vector<PendingBlock> edits;
	{
		std::lock_guard<std::mutex> lock(_pendingMutex);
		auto it = _pendingEdits.find(pos);
		if (it == _pendingEdits.end()) return;
		// Leave unfinished layers in their original order, ahead of any new
		// queued clicks. Only drain the layers whose terrain has arrived.
		auto& waiting = it->second;
		auto end = std::remove_if(waiting.begin(), waiting.end(), [&](const PendingBlock& edit) {
			if (chunk->needsGeneration(floor_div(edit.worldPos.y, CHUNK_SIZE))) return false;
			edits.push_back(edit);
			return true;
		});
		waiting.erase(end, waiting.end());
		if (waiting.empty()) _pendingEdits.erase(it);
	}
	for (const auto& edit : edits) {
		if (setBlock(pos, edit.worldPos, edit.value, edit.byPlayer) && edit.byPlayer)
			markBlockDirty(edit.worldPos, true);
	}
	if (!edits.empty()) scheduleDisplayUpdate();
}

// Single chunk loader
Chunk *ChunkLoader::loadChunk(int x, int z, int render, const ivec2 &chunkPos, int resolution)
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
		if (chunk->getResolution() > resolution) {
			std::lock_guard<std::recursive_mutex> editLock(_blockEditMutex);
			chunk->updateResolution(resolution);
			waterChunkLoaded(*chunk);
		}
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
			waterChunkLoaded(*chunk);
			chunk->getNeighbors();

			_chunksMemoryUsage.fetch_add(chunk->getMemorySize(), std::memory_order_relaxed);
			// Insert into LRU as most-recent entry
			touchLRU(pos);
			++_chunksCount;
			applyPendingFor(pos);
		}
	}

	bool displayInserted = false;
	{
		std::lock_guard<std::mutex> lk(_displayedChunksMutex);
		auto [it, inserted] = _displayedChunks.emplace(pos, chunk);
		if (!inserted) it->second = chunk;
		else {
			++_displayedCount;
			displayInserted = true;
		}
	}

	// If this chunk just became displayed again (from cache), refresh its flowers
	if (displayInserted) {
		rescanFlowersForChunk(pos);
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

// Allocate column metadata first; terrain is generated by the global layer queue.
Chunk* ChunkLoader::prepareStreamingChunk(ivec2 pos, int resolution) {
	Chunk* chunk = getChunk(pos);
	if (!chunk) {
		auto* map = _perlinGenerator.getPerlinMap(pos, resolution);
		auto created = std::make_unique<Chunk>(pos, map, _caveGen, *this, _threadPool, resolution);
		{
			std::lock_guard<std::mutex> lock(_chunksMutex);
			auto inserted = _chunks.emplace(pos, created.get());
			chunk = inserted.first->second;
			if (inserted.second) {
				created.release();
				++_chunksCount;
			}
		}
		chunk->getNeighbors(false);
	} else if (chunk->getResolution() > resolution) {
		std::lock_guard<std::mutex> buildLock(_displayBuildMutex);
		std::lock_guard<std::recursive_mutex> editLock(_blockEditMutex);
		size_t before = chunk->getMemorySize();
		chunk->updateResolution(resolution);
		if (chunk->getMemorySize() > before)
			_chunksMemoryUsage.fetch_add(chunk->getMemorySize() - before);
		waterChunkLoaded(*chunk);
	}
	bool redisplayed;
	{
		std::lock_guard<std::mutex> lock(_displayedChunksMutex);
		redisplayed = _displayedChunks.emplace(pos, chunk).second;
		if (redisplayed) ++_displayedCount;
	}
	touchLRU(pos);
	if (redisplayed && chunk->hasBlockData()) {
		rescanFlowersForChunk(pos);
		scheduleDisplayUpdate();
	}
	return chunk;
}

// Rank subchunks across all columns instead of finishing columns bottom-to-top.
void ChunkLoader::loadChunks(ivec2 chunkPos) {
	const int renderDistance = _renderDistance.load();
	if (renderDistance <= 0) return;
	_threshold = std::numeric_limits<int>::max();
	_currentRender = 0;
	const int radius = std::max(0, (renderDistance - 1) / 2);
	struct Candidate { Chunk* chunk; int layer; };
	std::vector<Candidate> pending;
	// Prepare nearer columns first so metadata work remains local as well.
	for (int ring = 0; ring <= radius && getIsRunning(); ++ring) {
		for (int dz = -ring; dz <= ring; ++dz) for (int dx = -ring; dx <= ring; ++dx) {
			if (std::max(std::abs(dx), std::abs(dz)) != ring) continue;
			int resolution = RESOLUTION;
			for (int threshold = LOD_THRESHOLD; ring >= threshold && resolution < CHUNK_SIZE;) {
				resolution = std::min(CHUNK_SIZE, resolution * 2);
				if (threshold > std::numeric_limits<int>::max() / 2) break;
				threshold *= 2;
			}
			auto* chunk = prepareStreamingChunk(chunkPos + ivec2(dx, dz), resolution);
			for (int layer = 0; layer <= chunk->maxGenerationLayer(); ++layer)
				if (chunk->needsGeneration(layer)) pending.push_back({chunk, layer});
		}
		if (hasMoved(chunkPos)) return;
	}
	// Queue only one small batch. Camera height/direction changes can reorder
	// everything remaining before the next batch, without stale FIFO world jobs.
	const size_t batchSize = std::max(size_t(1), std::min(size_t(4), _threadPool.workerCount()));
	while (!pending.empty() && getIsRunning()) {
		glm::vec3 eye = _camera.getWorldPosition();
		glm::vec3 forward = _camera.getDirection();
		Frustum frustum;
		bool hasFrustum;
		{
			std::lock_guard<std::mutex> lock(_frustumMutex);
			hasFrustum = _hasCachedFrustum;
			frustum = _cachedFrustum;
		}
		auto priority = [&](const Candidate& candidate) {
			auto pos = candidate.chunk->getPosition();
			return generationPriority({pos.x, candidate.layer, pos.y}, eye, forward,
			                          hasFrustum ? &frustum : nullptr);
		};
		size_t count = std::min(batchSize, pending.size());
		std::partial_sort(pending.begin(), pending.begin() + count, pending.end(),
			[&](const Candidate& a, const Candidate& b) { return priority(a) < priority(b); });
		std::vector<std::future<std::unique_ptr<SubChunk>>> futures;
		for (size_t i = 0; i < count; ++i) {
			auto task = pending[i];
			futures.emplace_back(_threadPool.enqueue([task] { return task.chunk->generateLayer(task.layer); }));
		}
		// Always join the active batch before movement cancellation/shutdown.
		for (size_t i = 0; i < count; ++i) {
			auto generated = futures[i].get();
			auto task = pending[i];
			std::lock_guard<std::mutex> buildLock(_displayBuildMutex);
			std::lock_guard<std::recursive_mutex> editLock(_blockEditMutex);
			size_t before = task.chunk->getMemorySize();
			task.chunk->installLayer(task.layer, std::move(generated));
			_chunksMemoryUsage.fetch_add(task.chunk->getMemorySize() - before);
			waterChunkLoaded(*task.chunk, task.layer);
			applyPendingFor(task.chunk->getPosition());
		}
		pending.erase(pending.begin(), pending.begin() + count);
		scheduleDisplayUpdate();
		if (hasMoved(chunkPos)) break;
	}
	_currentRender = renderDistance;
	enforceCountBudget();
}

void ChunkLoader::unloadChunks(ivec2 newCamChunk)
{

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
			if (std::abs(chunkPos.x - newCamChunk.x) > _renderDistance.load(std::memory_order_relaxed) / 2
				|| std::abs(chunkPos.y - newCamChunk.y) > _renderDistance.load(std::memory_order_relaxed) / 2)
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
			// Allow re-scan of flowers when this chunk becomes displayed again
			clearFlowerScanMarksFor(key);
		}
	}
	scheduleDisplayUpdate();
	// After display set shrinks, re-check cache pressure
	enforceCountBudget();
}

// Check if player moved
bool ChunkLoader::hasMoved(const ivec2 &oldPos)
{
	// Debounce movement: only signal a cancel if movement exceeds 1 chunk
	// away from the load center. This avoids restarting rings too aggressively
	// when sprinting or moving quickly.
	ivec2 camChunk = _camera.getChunkPosition(CHUNK_SIZE);
	int dx = std::abs((int)std::floor(oldPos.x) - (int)std::floor(camChunk.x));
	int dz = std::abs((int)std::floor(oldPos.y) - (int)std::floor(camChunk.y));
	return (std::max(dx, dz) > 1);
}

// Update data to be rendered
void ChunkLoader::updateFillData(bool incremental)
{
	std::lock_guard<std::mutex> buildLock(_displayBuildMutex);
	// If the engine is shutting down, skip any deferred heavy work
	if (!getIsRunning()) {
		return;
	}
	// Avoid piling up heavy builds and starving the render thread
	if (_buildingDisplay.exchange(true))
		return;
	std::vector<ivec2> pending;
	{
		std::lock_guard<std::mutex> lock(_pendingMutex);
		for (const auto& entry : _pendingEdits) pending.push_back(entry.first);
	}
	for (auto pos : pending) {
		auto* chunk=getChunk(pos);
		if (chunk && !chunk->isBuilding()) applyPendingFor(pos);
	}
	flushDirtyChunks(incremental);
	DisplayData *fillData = new DisplayData();
	DisplayData *transparentData = new DisplayData();
	buildFacesToDisplay(fillData, transparentData);
	// If the build produced no draw commands at all, skip publishing this
	// empty frame to avoid wiping renderer buffers and causing a blank frame.
	if (fillData->indirectBufferData.empty() && transparentData->indirectBufferData.empty())
	{
		delete fillData;
		delete transparentData;
	}
	else
	{
		std::lock_guard<std::mutex> lk(_sharedDrawDataMutex);
		// Replace stale staged snapshots so edits are visible on the next upload.
		while (!_solidStagedDataQueue.empty()) {
			delete _solidStagedDataQueue.front(); _solidStagedDataQueue.pop();
		}
		while (!_transparentStagedDataQueue.empty()) {
			delete _transparentStagedDataQueue.front(); _transparentStagedDataQueue.pop();
		}
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
	size_t totalSSBOS      = 0;
	size_t totalSSBOT      = 0;
	size_t totalMetaS      = 0;
	size_t totalMetaT      = 0;
	for (const auto& c : snapshot) {
		if (!getIsRunning())
			return ;
		auto sizes = c->displaySizes();
		totalSolidVerts += sizes[0]; totalTransVerts += sizes[1];
		totalSolidCmds += sizes[2]; totalTransCmds += sizes[3];
		totalSSBOS += sizes[4]; totalSSBOT += sizes[5];
		totalMetaS += sizes[2]; totalMetaT += sizes[3];
	}
	fillData->vertexData.reserve(fillData->vertexData.size() + totalSolidVerts);
	fillData->indirectBufferData.reserve(fillData->indirectBufferData.size() + totalSolidCmds);
	fillData->ssboData.reserve(fillData->ssboData.size() + totalSSBOS);
	transparentFillData->vertexData.reserve(transparentFillData->vertexData.size() + totalTransVerts);
	transparentFillData->indirectBufferData.reserve(transparentFillData->indirectBufferData.size() + totalTransCmds);
	transparentFillData->ssboData.reserve(transparentFillData->ssboData.size() + totalSSBOT);
	fillData->drawMeta.reserve(fillData->drawMeta.size() + totalMetaS);
	transparentFillData->drawMeta.reserve(transparentFillData->drawMeta.size() + totalMetaT);

	// Build a fresh visible subchunk snapshot locally, then swap it in atomically
	std::unordered_map<glm::ivec2, std::unordered_set<int>, ivec2_hash> nextDisplayedSubY;

	for (const auto& c : snapshot) {
		if (!getIsRunning())
			return ;

		// Take a coherent snapshot of this chunk's display data under its internal lock
		std::vector<int> sVerts, tVerts;
		std::vector<DrawArraysIndirectCommand> sCmds, tCmds;
		std::vector<vec4> ssboS, ssboT;
		std::vector<uint32_t> metaS, metaT;
		c->snapshotDisplayData(sVerts, sCmds, ssboS, tVerts, tCmds, ssboT, metaS, metaT);

		// SOLID
		size_t vSizeBefore = fillData->vertexData.size();
		if (!sVerts.empty())
			fillData->vertexData.insert(fillData->vertexData.end(), sVerts.begin(), sVerts.end());
		for (size_t i = 0; getIsRunning() && i < sCmds.size(); ++i) {
			auto cmd = sCmds[i];
			cmd.baseInstance += (uint32_t)vSizeBefore;
			fillData->indirectBufferData.push_back(cmd);
		}
		if (!ssboS.empty())
			fillData->ssboData.insert(fillData->ssboData.end(), ssboS.begin(), ssboS.end());
		if (!metaS.empty())
			fillData->drawMeta.insert(fillData->drawMeta.end(), metaS.begin(), metaS.end());

		// Plants have their own mesh, so a plant-only subchunk has no terrain
		// draw commands. Include all loaded layers once this chunk has terrain
		// in the display snapshot (also allowing transparent-only terrain).
		if (!sCmds.empty() || !tCmds.empty()) {
			std::vector<int> subs;
			c->getSubIndices(subs);
			auto &displayed = nextDisplayedSubY[c->getPosition()];
			displayed.insert(subs.begin(), subs.end());
		}

		// TRANSPARENT
		size_t tvBefore = transparentFillData->vertexData.size();
		if (!tVerts.empty())
			transparentFillData->vertexData.insert(
				transparentFillData->vertexData.end(), tVerts.begin(), tVerts.end()
			);
		for (size_t i = 0; getIsRunning() && i < tCmds.size(); ++i) {
			auto cmd = tCmds[i];
			cmd.baseInstance += (uint32_t)tvBefore;
			transparentFillData->indirectBufferData.push_back(cmd);
		}
		if (!ssboT.empty())
			transparentFillData->ssboData.insert(transparentFillData->ssboData.end(), ssboT.begin(), ssboT.end());
		if (!metaT.empty())
			transparentFillData->drawMeta.insert(transparentFillData->drawMeta.end(), metaT.begin(), metaT.end());
	}

	// Publish the freshly built visible subchunk snapshot atomically
	{
		std::lock_guard<std::mutex> lk(_sharedDrawDataMutex);
		_lastDisplayedSubY.swap(nextDisplayedSubY);
	}
}

// Dirty chunks management
void ChunkLoader::flushDirtyChunks(bool incremental) {
	// Snapshot candidates before ranking: never inspect chunk/subchunk state
	// while holding the dirty lock used by concurrent edits and generation.
	std::vector<std::pair<ivec2, std::set<int>>> candidates;
	if (incremental) {
		std::lock_guard<std::mutex> lock(_dirtyMutex);
		if (_urgentLayers.empty()) {
			for (auto pos : _dirtyChunks) {
				auto layers = _dirtyLayers.find(pos);
				candidates.emplace_back(pos, layers == _dirtyLayers.end() ?
				                        std::set<int>{} : layers->second);
			}
		}
	}
	ivec2 next(0);
	bool haveNext = false;
	GenerationPriority best{};
	if (!candidates.empty()) {
		auto eye = _camera.getWorldPosition();
		auto forward = _camera.getDirection();
		Frustum frustum;
		bool hasFrustum;
		{
			std::lock_guard<std::mutex> lock(_frustumMutex);
			frustum = _cachedFrustum;
			hasFrustum = _hasCachedFrustum;
		}
		for (const auto& candidate : candidates) {
			auto* chunk = getChunk(candidate.first);
			if (!chunk || !chunk->hasBlockData()) continue;
			std::vector<int> layers;
			chunk->getSubIndices(layers);
			for (int layer : layers) {
				if (!candidate.second.empty() && !candidate.second.count(layer)) continue;
				// Dirty neighboring layers may not have arrived yet. They must
				// not promote a distant column using geometry it cannot display.
				if (chunk->needsGeneration(layer)) continue;
				auto priority = generationPriority({candidate.first.x, layer, candidate.first.y},
				                                   eye, forward, hasFrustum ? &frustum : nullptr);
				if (!haveNext || priority < best) {
					haveNext = true;
					best = priority;
					next = candidate.first;
				}
			}
		}
	}

	std::vector<std::pair<ivec2, std::set<int>>> jobs;
	bool more = false;
	{
		std::lock_guard<std::mutex> lk(_dirtyMutex);
		if (incremental && !_urgentLayers.empty()) {
			// Publish player changes together before unrelated background columns.
			for (auto& entry : _urgentLayers) {
				auto layers = std::move(entry.second);
				auto dirty = _dirtyLayers.find(entry.first);
				if (dirty != _dirtyLayers.end()) {
					layers.insert(dirty->second.begin(), dirty->second.end());
					_dirtyLayers.erase(dirty);
					_dirtyChunks.erase(entry.first);
				}
				// Keep a queued full rebuild: it may contain generation changes
				// outside the edited layers.
				jobs.emplace_back(entry.first, std::move(layers));
			}
			_urgentLayers.clear();
		} else {
			while (!_dirtyChunks.empty()) {
				auto pos = incremental && haveNext ? next : *_dirtyChunks.begin();
				std::set<int> layers;
				auto dirty = _dirtyLayers.find(pos);
				if (dirty != _dirtyLayers.end()) {
					layers = std::move(dirty->second);
					_dirtyLayers.erase(dirty);
				}
				jobs.emplace_back(pos, std::move(layers));
				_dirtyChunks.erase(pos);
				_urgentLayers.erase(pos);
				// Bound background work so an edit never waits for a whole world batch.
				if (incremental) break;
			}
		}
		more = !_dirtyChunks.empty() || !_urgentLayers.empty();
	}
	for (const auto& job : jobs) {
		if (auto c = getChunk(job.first))
			c->sendFacesToDisplay(job.second.empty() ? nullptr : &job.second);
	}
	if (incremental && more) scheduleDisplayUpdate();
}

void ChunkLoader::markChunkDirty(const ivec2& pos) {
	std::lock_guard<std::mutex> lk(_dirtyMutex);
	_dirtyChunks.insert(pos);
	_dirtyLayers.erase(pos);
}

void ChunkLoader::markLayerDirty(const ivec2& pos, int layer) {
	std::lock_guard<std::mutex> lock(_dirtyMutex);
	for (int dx = -1; dx <= 1; ++dx) for (int dz = -1; dz <= 1; ++dz) {
		ivec2 neighbor = pos + ivec2(dx, dz);
		bool inserted = _dirtyChunks.insert(neighbor).second;
		if (inserted || _dirtyLayers.count(neighbor))
			for (int dy = -1; dy <= 1; ++dy) _dirtyLayers[neighbor].insert(layer + dy);
	}
}

void ChunkLoader::markBlockDirty(const ivec3& worldPos, bool urgent) {
	std::lock_guard<std::mutex> lk(_dirtyMutex);
	// Faces and liquid corner heights sample adjacent cells, including diagonals.
	// Include vertical neighbors at subchunk boundaries as well.
	for (int dx = -1; dx <= 1; ++dx)
		for (int dz = -1; dz <= 1; ++dz) {
			ivec2 pos(floor_div(worldPos.x + dx, CHUNK_SIZE),
			          floor_div(worldPos.z + dz, CHUNK_SIZE));
			bool inserted = _dirtyChunks.insert(pos).second;
			for (int dy = -1; dy <= 1; ++dy) {
				int layer = floor_div(worldPos.y + dy, CHUNK_SIZE);
				if (inserted || _dirtyLayers.count(pos)) _dirtyLayers[pos].insert(layer);
				if (urgent) _urgentLayers[pos].insert(layer);
			}
		}
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
	// Floor before resolving local coordinates so negative Y cannot alias subchunk 0.
	int subChunkIndex = floor_div(worldPos.y, CHUNK_SIZE);
	SubChunk *subchunk = chunk->getSubChunk(subChunkIndex);
	if (!subchunk) return AIR;
	int localX = (worldPos.x % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;
	int localY = (worldPos.y % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;
	int localZ = (worldPos.z % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;
	return subchunk->getBlock({localX, localY, localZ});
}

// Shared chunk setters
bool ChunkLoader::setBlock(ivec2 chunkPos, ivec3 worldPos, BlockType value, bool byPlayer) {
    std::lock_guard<std::recursive_mutex> editLock(_blockEditMutex);
	auto chunk = getChunk(chunkPos);
	if (!chunk) return false;

	const int subY = (int)std::floor((double)worldPos.y / (double)CHUNK_SIZE);
	SubChunk* sc = chunk->getOrCreateSubChunk(subY, /*generate=*/false);
	if (!sc) return false;

	const int lx = (worldPos.x % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;
	const int ly = (worldPos.y % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;
	const int lz = (worldPos.z % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;

	const char previous = sc->getBlock({lx, ly, lz});
	if (previous == value) return true;
	// Write directly with localized coordinates to avoid re-dispatch loops
	sc->setBlockLocal(lx, ly, lz, value);
	WaterPos lavaPos{worldPos.x,worldPos.y,worldPos.z};
	bool lightingChanged=isLava(previous) || isLava(value);
	if (isLava(value)) _lavaBlocks.insert(lavaPos); else _lavaBlocks.erase(lavaPos);
	for (auto d:{WaterPos{1,0,0},WaterPos{-1,0,0},WaterPos{0,1,0},WaterPos{0,-1,0},WaterPos{0,0,1},WaterPos{0,0,-1}})
		lightingChanged=lightingChanged || _lavaBlocks.count({worldPos.x+d[0],worldPos.y+d[1],worldPos.z+d[2]});
	if (lightingChanged) ++_lavaLightRevision;


	// Only player actions count as modifications for eviction protection UI
	if (byPlayer && !chunk->getModified()) { chunk->setAsModified(); ++_modifiedCount; }
	markBlockDirty(worldPos);
	queueWater(worldPos);
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

void ChunkLoader::snapshotDebugCounters() {
	// Called from main thread before UI render
	_dbg_chunksMemoryUsage = _chunksMemoryUsage.load(std::memory_order_relaxed);
	_dbg_renderDistance   = _renderDistance.load(std::memory_order_relaxed);
	_dbg_currentRender    = _currentRender.load(std::memory_order_relaxed);
	_dbg_chunksCount      = _chunksCount.load(std::memory_order_relaxed);
	_dbg_displayedCount   = _displayedCount.load(std::memory_order_relaxed);
	_dbg_modifiedCount    = _modifiedCount.load(std::memory_order_relaxed);
}

int *ChunkLoader::getCurrentRenderPtr() { return &_dbg_currentRender; }
size_t *ChunkLoader::getMemorySizePtr() { return &_dbg_chunksMemoryUsage; }
int *ChunkLoader::getRenderDistancePtr() { return &_dbg_renderDistance; }
int *ChunkLoader::getCachedChunksCountPtr() { return &_dbg_chunksCount; }
int *ChunkLoader::getDisplayedChunksCountPtr() { return &_dbg_displayedCount; }
int *ChunkLoader::getModifiedChunksCountPtr() { return &_dbg_modifiedCount; }
NoiseGenerator &ChunkLoader::getNoiseGenerator() { return _perlinGenerator; }

void ChunkLoader::printSizes() const
{
	{
		// Intentionally left blank: debug size prints removed
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
	// Meshing takes the chunk's display-data lock before plant discovery
	// checks the displayed set. Release the set lock before inspecting meshes
	// so the startup readiness check cannot take these locks in reverse order.
	std::vector<Chunk*> snapshot;
	{
		std::lock_guard<std::mutex> lk(_displayedChunksMutex);
		snapshot.reserve(_displayedChunks.size());
		for (const auto &kv : _displayedChunks) snapshot.push_back(kv.second);
	}
	for (Chunk* c : snapshot) {
		if (!c) continue;
		if (!c->isReady()) continue;
		// Consider renderable if any indirect commands exist (solid or transparent)
		// Use thread-safe predicate to avoid reading vectors concurrently
		if (c->hasAnyDraws())
			return true;
	}
	return false;
}

void ChunkLoader::scheduleDisplayUpdate() {
	if (!getIsRunning()) return;
	{
		std::lock_guard<std::mutex> lock(_displayWakeMutex);
		_displayRequested = true;
	}
	_displayWake.notify_one();
}

void ChunkLoader::rebuildDisplayDataNow()
{
	// Build staged data synchronously; guards inside will noop if shutting down
	updateFillData();
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
	int renderCells = _renderDistance.load(std::memory_order_relaxed) * _renderDistance.load(std::memory_order_relaxed);
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
	std::lock_guard<std::mutex> buildLock(_displayBuildMutex);
    std::lock_guard<std::recursive_mutex> editLock(_blockEditMutex);
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

	// Drop any staged/planted flower data for this chunk to prevent
	// unbounded growth while exploring.
	{
		std::lock_guard<std::mutex> fk(_flowersMutex);
		// Remove any still-staged discoveries for this chunk
		_discoveredFlowers.erase(candidate);
		// Remove scanned-keys for all subY belonging to this chunk
		const std::string prefix = std::to_string(candidate.x) + ":" + std::to_string(candidate.y) + ":";
		for (auto it = _flowersScannedKeys.begin(); it != _flowersScannedKeys.end(); ) {
			if (it->rfind(prefix, 0) == 0) it = _flowersScannedKeys.erase(it);
			else ++it;
		}
	}

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
	for (auto it=_lavaBlocks.begin();it!=_lavaBlocks.end();) {
		if (floor_div((*it)[0],CHUNK_SIZE)==candidate.x && floor_div((*it)[2],CHUNK_SIZE)==candidate.y) it=_lavaBlocks.erase(it);
		else ++it;
	}
	++_lavaLightRevision;
	ivec2 chunkPos = chunk->getPosition();
	_perlinGenerator.removePerlinMap(chunkPos.x, chunkPos.y);
	chunk->freeSubChunks();
	delete chunk;
	if (_chunksMemoryUsage.load(std::memory_order_relaxed) >= freed)
		_chunksMemoryUsage.fetch_sub(freed, std::memory_order_relaxed);
	else
		_chunksMemoryUsage = 0;
	return true;
}

// --- Flowers discovery helpers ---
void ChunkLoader::scanAndRecordFlowersFor(const glm::ivec2& cpos, int subY, SubChunk* sc, int resolution)
{
	if (!sc) return;
	// Neighbor linking can mesh a chunk before loadChunk registers it for
	// display. Publishing plants then lets the renderer prune them as absent,
	// while the scan mark prevents their discovery after registration.
	{
		std::lock_guard<std::mutex> lk(_displayedChunksMutex);
		if (_displayedChunks.find(cpos) == _displayedChunks.end()) return;
	}
	// Avoid scanning the same sublayer multiple times
	const std::string key = _flowerKey(cpos, subY);
	{
		std::lock_guard<std::mutex> lk(_flowersMutex);
		if (_flowersScannedKeys.find(key) != _flowersScannedKeys.end()) return;
		_flowersScannedKeys.insert(key);
	}

	// Iterate the subchunk cells at its resolution and record any FLOWER blocks
	std::vector<std::pair<glm::ivec3, BlockType>> flowers;
	flowers.reserve(8);
	for (int z = 0; z < CHUNK_SIZE; z += resolution) {
		for (int y = 0; y < CHUNK_SIZE; y += resolution) {
			for (int x = 0; x < CHUNK_SIZE; x += resolution) {
				BlockType b = sc->getBlock({x, y, z});
				if (b == FLOWER_POPPY || b == FLOWER_DANDELION || b == FLOWER_CYAN || b == FLOWER_SHORT_GRASS || b == FLOWER_DEAD_BUSH) {
					// Convert to absolute world cell
					glm::ivec3 cell(
						x + cpos.x * CHUNK_SIZE,
						y + subY   * CHUNK_SIZE,
						z + cpos.y * CHUNK_SIZE
					);
					flowers.emplace_back(cell, b);
				}
			}
		}
	}
	if (!flowers.empty()) {
		std::lock_guard<std::mutex> lk(_flowersMutex);
		auto &bySub = _discoveredFlowers[cpos];
		auto &vec = bySub[subY];
		vec.insert(vec.end(), flowers.begin(), flowers.end());
	}
}

void ChunkLoader::fetchAndClearDiscoveredFlowers(std::vector<std::tuple<glm::ivec2,int,glm::ivec3,BlockType>>& out)
{
	std::lock_guard<std::mutex> lk(_flowersMutex);
	for (auto &kv : _discoveredFlowers) {
		const glm::ivec2 cpos = kv.first;
		for (auto &kv2 : kv.second) {
			int subY = kv2.first;
			auto &lst = kv2.second;
			for (auto &p : lst) {
				out.emplace_back(cpos, subY, p.first, p.second);
			}
		}
	}
	_discoveredFlowers.clear();
}

void ChunkLoader::getDisplayedSubchunksSnapshot(std::unordered_map<glm::ivec2, std::unordered_set<int>, ivec2_hash>& out)
{
	std::lock_guard<std::mutex> lk(_sharedDrawDataMutex);
	out = _lastDisplayedSubY;
}

void ChunkLoader::clearFlowerScanMarksFor(const glm::ivec2& cpos)
{
	std::lock_guard<std::mutex> lk(_flowersMutex);
	const std::string prefix = std::to_string(cpos.x) + ":" + std::to_string(cpos.y) + ":";
	for (auto it = _flowersScannedKeys.begin(); it != _flowersScannedKeys.end(); ) {
		if (it->rfind(prefix, 0) == 0) it = _flowersScannedKeys.erase(it);
		else ++it;
	}
}

void ChunkLoader::rescanFlowersForChunk(const glm::ivec2& cpos)
{
	Chunk* c = getChunk(cpos);
	if (!c) return;
	if (c->getResolution() != 1) return; // only scan at full-res
	std::vector<int> subs; c->getSubIndices(subs);
	for (int subY : subs) {
		SubChunk* sc = c->getSubChunk(subY);
		if (sc) scanAndRecordFlowersFor(cpos, subY, sc, c->getResolution());
	}
}

void ChunkLoader::queueWater(const glm::ivec3& p) {
    std::lock_guard<std::mutex> lock(_waterMutex);
    auto schedule = [&](glm::ivec3 q) {
        glm::ivec2 cp(floor_div(q.x, CHUNK_SIZE), floor_div(q.z, CHUNK_SIZE));
        char block=getBlock(cp,q);
        if (isLiquid(block)) _waterQueue.schedule({q.x,q.y,q.z},isLava(block) ? LAVA_TICK_DELAY : WATER_TICK_DELAY);
    };
    schedule(p);
    for (auto d : WATER_SIDES) schedule({p.x+d[0],p.y,p.z+d[2]});
    schedule({p.x,p.y+1,p.z});
    schedule({p.x,p.y-1,p.z});
}

void ChunkLoader::waterChunkLoaded(Chunk& chunk, int layer) {
    const bool singleLayer = layer != std::numeric_limits<int>::min();
    std::lock_guard<std::recursive_mutex> editLock(_blockEditMutex);
    if (chunk.getResolution() != 1 || !chunk.hasBlockData()) return;
    auto cp = chunk.getPosition();
    for (auto it=_lavaBlocks.begin();it!=_lavaBlocks.end();) {
        if (floor_div((*it)[0],CHUNK_SIZE)==cp.x && floor_div((*it)[2],CHUNK_SIZE)==cp.y &&
            (!singleLayer || floor_div((*it)[1],CHUNK_SIZE)==layer)) it=_lavaBlocks.erase(it);
        else ++it;
    }
    ++_lavaLightRevision;
    {
        std::lock_guard<std::mutex> lock(_waterMutex);
        auto it = _waterWaiting.find({cp.x,cp.y});
        if (it != _waterWaiting.end()) {
            for (auto p : it->second) {
                char block=getBlock({floor_div(p[0],32),floor_div(p[2],32)},{p[0],p[1],p[2]});
                _waterQueue.schedule(p,isLava(block) ? LAVA_TICK_DELAY : WATER_TICK_DELAY);
            }
            _waterWaiting.erase(it);
        }
    }
    // One-time initialization, never a per-frame scan. Interior ocean sources
    // cannot spread; only exposed and boundary water needs an initial tick.
    std::vector<int> subs;
    if (singleLayer) subs.push_back(layer);
    else chunk.getSubIndices(subs);
    std::sort(subs.begin(), subs.end());
    for (int sy : subs) {
        auto* sc = chunk.getSubChunk(sy);
        for (int y = 0; y < CHUNK_SIZE; ++y)
        for (int z = 0; z < CHUNK_SIZE; ++z)
        for (int x = 0; x < CHUNK_SIZE; ++x) {
            char b = sc->getBlock({x,y,z});
            if (isLava(b)) _lavaBlocks.insert({cp.x*32+x,sy*32+y,cp.y*32+z});
            if (!isLiquid(b)) continue;
            glm::ivec3 p(cp.x*CHUNK_SIZE+x, sy*CHUNK_SIZE+y, cp.y*CHUNK_SIZE+z);
            bool active = b != (isLava(b) ? LAVA : WATER) || x == 0 || z == 0 ||
                x == CHUNK_SIZE-1 || z == CHUNK_SIZE-1;
            for (auto d : {glm::ivec3(1,0,0), glm::ivec3(-1,0,0),
                glm::ivec3(0,0,1), glm::ivec3(0,0,-1), glm::ivec3(0,-1,0)}) {
                auto q = p+d;
                if (floor_div(q.x, CHUNK_SIZE) != cp.x ||
                    floor_div(q.z, CHUNK_SIZE) != cp.y) continue;
                char neighbor = getBlock(cp, q);
                active = active || (waterReplaceable(neighbor) && !isWater(neighbor));
            }
            if (active) {
                std::lock_guard<std::mutex> lock(_waterMutex);
                _waterQueue.schedule({p.x,p.y,p.z},isLava(b) ? LAVA_TICK_DELAY : WATER_TICK_DELAY);
            }
        }
    }
    // Diagonal surface samples must refresh when formerly missing data arrives.
    if (singleLayer) markLayerDirty(cp,layer);
    else for (int dx = -1; dx <= 1; ++dx)
        for (int dz = -1; dz <= 1; ++dz) markChunkDirty({cp.x+dx,cp.y+dz});
}

void ChunkLoader::stepWater(bool deferWhenBusy) {
    std::unique_lock<std::recursive_mutex> editLock(_blockEditMutex, std::defer_lock);
    // Refinement/initialization may hold this lock for a whole chunk build.
    // Leave the queue intact and retry on the next game tick instead of
    // making the renderer wait for generation or lighting collection.
    if (deferWhenBusy) {
        if (!editLock.try_lock()) return;
    } else editLock.lock();
    std::vector<WaterPos> batch;
    {
        std::lock_guard<std::mutex> lock(_waterMutex);
        batch = _waterQueue.advance();
    }
    auto chunkAt = [](WaterPos p) {
        return glm::ivec2(floor_div(p[0], CHUNK_SIZE), floor_div(p[2], CHUNK_SIZE));
    };
    for (auto p : batch) {
        std::map<WaterPos, char> cache;
        std::set<std::array<int, 2>> missing;
        auto read = [&](WaterPos q) -> char {
            if (q[1] <= 0) return BEDROCK;
            auto found = cache.find(q);
            if (found != cache.end()) return found->second;
            auto cp = chunkAt(q);
            Chunk* c = getChunk(cp);
            char b = WATER_UNKNOWN;
            if (!c || !c->hasBlockData() || c->getResolution() != 1 ||
                c->needsGeneration(floor_div(q[1],CHUNK_SIZE)))
                missing.insert({cp.x,cp.y});
            else b = getBlock(cp, {q[0],q[1],q[2]});
            cache[q] = b;
            return b;
        };
        std::map<WaterPos, char> changes;
        auto write = [&](WaterPos q, char b) {
            changes[q] = b;
            cache[q] = b;
        };
        if (isLava(read(p))) processLavaTick(read,write,p);
        else processWaterTick(read, write, p, _waterSourceConversion);
        if (!missing.empty()) {
            std::lock_guard<std::mutex> lock(_waterMutex);
            for (auto cp : missing) _waterWaiting[cp].insert(p);
        } else {
            for (auto [q,b] : changes)
                setBlock(chunkAt(q), {q[0],q[1],q[2]}, b, true);
        }
    }
}

void ChunkLoader::setWaterSourceConversion(bool enabled) {
    std::lock_guard<std::recursive_mutex> lock(_blockEditMutex);
    _waterSourceConversion = enabled;
}

size_t ChunkLoader::pendingWaterUpdates() {
    std::lock_guard<std::mutex> lock(_waterMutex);
    return _waterQueue.size();
}

// Simulation must run even when a display snapshot is waiting for the renderer.
// The game's fixed-step accumulator calls this once for every elapsed tick.
void ChunkLoader::updateWaterTick() {
    stepWater(true);
    bool dirty;
    {
        std::lock_guard<std::mutex> lock(_dirtyMutex);
        dirty = !_dirtyChunks.empty();
    }
    // Retry publication on later ticks if a previous snapshot is still staged.
    if (dirty) scheduleDisplayUpdate();
}

// A bounded debug snapshot; disabled mode never calls this or scans water.
void ChunkLoader::waterDebugSlice(glm::ivec3 center, std::array<std::string,17>& rows) {
    std::lock_guard<std::recursive_mutex> lock(_blockEditMutex);
    for (int z = -8; z <= 8; ++z) {
        auto& row = rows[z+8];
        row.clear();
        for (int x = -8; x <= 8; ++x) {
            auto p = center + glm::ivec3(x,0,z);
            glm::ivec2 cp(floor_div(p.x,CHUNK_SIZE),floor_div(p.z,CHUNK_SIZE));
            auto c = getChunk(cp);
            char b = c && c->hasBlockData() && c->getResolution() == 1 ? getBlock(cp,p) : WATER_UNKNOWN;
            std::string label = isLava(b) ? "L" : b == WATER ? "S" : b == WATER_FALLING ? "F8" :
                isWater(b) ? std::to_string(waterAmount(b)) : b == WATER_UNKNOWN ? "?" :
                waterReplaceable(b) ? "." : "#";
            row += label + std::string(3-label.size(),' ');
        }
    }
}

std::vector<LavaEmitter> ChunkLoader::lavaEmitters() {
    std::lock_guard<std::recursive_mutex> lock(_blockEditMutex);
    std::vector<LavaEmitter> emitters;
    auto read=[&](WaterPos p) {
        ivec2 cp{floor_div(p[0],32),floor_div(p[2],32)};
        auto* chunk=getChunk(cp);
        if (!chunk || !chunk->hasBlockData() || chunk->getResolution()!=1) return char(WATER_UNKNOWN);
        return getBlock(cp,{p[0],p[1],p[2]});
    };
    for (auto p:_lavaBlocks) {
        char block=read(p);
        if (!isLava(block)) continue;
        float height=isLava(read({p[0],p[1]+1,p[2]})) ? 1.0f : lavaHeight(block)/15.0f;
        for (auto d:{WaterPos{1,0,0},WaterPos{-1,0,0},WaterPos{0,1,0},WaterPos{0,-1,0},WaterPos{0,0,1},WaterPos{0,0,-1}}) {
            char adjacent=read({p[0]+d[0],p[1]+d[1],p[2]+d[2]});
            if (adjacent==WATER_UNKNOWN || isLava(adjacent)) continue;
            Direction direction=d[1]==1 ? UP : d[1]==-1 ? DOWN : NORTH;
            bool exposed=faceDisplayCondition(LAVA,adjacent,direction);
            if (exposed) emitters.push_back({p[0]+0.5f+d[0]*0.5f,p[1]+(d[1]==1 ? height : d[1]==-1 ? 0.0f : height*0.5f),p[2]+0.5f+d[2]*0.5f,0});
        }
    }
    return emitters;
}
