#include "World.hpp"
#include "define.hpp"

World::World(int seed, TextureManager &textureManager, Camera &camera, ThreadPool &pool) : _threadPool(pool), _textureManager(textureManager), _camera(camera), _perlinGenerator(seed), _caveGen(1000, 0.01f, 0.05f, 0.6f, 0.6f, 42)
{
	_needUpdate = true;
	_needTransparentUpdate = true;
	_hasBufferInitialized = false;
	_renderDistance = RENDER_DISTANCE;
	_currentRender = 0;

	_memorySize = 0;

	_drawData = nullptr;
	_transparentDrawData = nullptr;

	// Load first chunk under the player, and pop their position on top of it
	ivec2 chunkPos = camera.getChunkPosition(CHUNK_SIZE);
	vec3 worldPos = camera.getWorldPosition();
	loadChunk(0, 0, 0, chunkPos, 1);
	TopBlock topBlock = findTopBlockY(chunkPos, {worldPos.x, worldPos.z});
	const vec3 &camPos = camera.getPosition();
	// Place camera: feet on ground
	camera.setPos({camPos.x - 0.5, -(topBlock.height + 1 + EYE_HEIGHT), camPos.z - 0.5});
}

TopBlock World::findTopBlockY(ivec2 chunkPos, ivec2 worldPos) {
	std::lock_guard<std::mutex> lock(_chunksMutex);
	Chunk* chunk = _chunks[{chunkPos.x, chunkPos.y}];
	if (!chunk) return TopBlock();
	int localX = (worldPos.x % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;
	int localZ = (worldPos.y % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;
	TopBlock topBlock;
	topBlock = chunk->getTopBlock(localX, localZ);
	return topBlock;
}

static inline int mod_floor(int a, int b) {
	int r = a % b;
	return (r < 0) ? r + b : r;
}
static inline int floor_div(int a, int b) {
	int q = a / b, r = a % b;
	return (r && ((r < 0) != (b < 0))) ? (q - 1) : q;
}

TopBlock World::findBlockUnderPlayer(ivec2 chunkPos, ivec3 worldPos) {
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

void World::init(int renderDistance = RENDER_DISTANCE) {
	_renderDistance = renderDistance;
	initGLBuffer();
}

World::~World()
{
	std::lock_guard<std::mutex> lock(_chunksMutex);
	for (auto it = _chunks.begin(); it != _chunks.end(); it++)
	{
		it->second->freeSubChunks();
		delete it->second;
	}
	while (_stagedDataQueue.size())
	{
		auto &tmp = _stagedDataQueue.front();
		_stagedDataQueue.pop();
		delete tmp;
	}
	while (_transparentStagedDataQueue.size())
	{
		auto &tmp = _transparentStagedDataQueue.front();
		_transparentStagedDataQueue.pop();
		delete tmp;
	}
	delete _drawData;
	delete _transparentDrawData;
}

void World::shutdownGL()
{
	// Delete GL programs and buffers created by World
	if (_cullProgram) { glDeleteProgram(_cullProgram); _cullProgram = 0; }
	if (_vao) { glDeleteVertexArrays(1, &_vao); _vao = 0; }
	if (_transparentVao) { glDeleteVertexArrays(1, &_transparentVao); _transparentVao = 0; }

	if (_vbo) { glDeleteBuffers(1, &_vbo); _vbo = 0; }
	if (_indirectBuffer) { glDeleteBuffers(1, &_indirectBuffer); _indirectBuffer = 0; }

	if (_transparentIndirectBuffer) { glDeleteBuffers(1, &_transparentIndirectBuffer); _transparentIndirectBuffer = 0; }

	if (_templIndirectBuffer) { glDeleteBuffers(1, &_templIndirectBuffer); _templIndirectBuffer = 0; }
	if (_transpTemplIndirectBuffer) { glDeleteBuffers(1, &_transpTemplIndirectBuffer); _transpTemplIndirectBuffer = 0; }
	if (_frustumUBO) { glDeleteBuffers(1, &_frustumUBO); _frustumUBO = 0; }

	if (_solidPosSSBO) { glDeleteBuffers(1, &_solidPosSSBO); _solidPosSSBO = 0; }
	if (_transpPosSSBO) { glDeleteBuffers(1, &_transpPosSSBO); _transpPosSSBO = 0; }
	if (_solidInstSSBO) { glDeleteBuffers(1, &_solidInstSSBO); _solidInstSSBO = 0; }
	if (_transpInstSSBO) { glDeleteBuffers(1, &_transpInstSSBO); _transpInstSSBO = 0; }
}

NoiseGenerator &World::getNoiseGenerator()
{
	return (_perlinGenerator);
}

int *World::getRenderDistancePtr()
{
	return &_renderDistance;
}

BlockType World::getBlock(ivec2 chunkPos, ivec3 worldPos)
{
	std::lock_guard<std::mutex> lock(_chunksMutex);
	Chunk *chunk = _chunks[{chunkPos.x, chunkPos.y}];
	if (!chunk)
		return AIR;
	int subChunkIndex = static_cast<int>(floor(worldPos.y / CHUNK_SIZE));
	SubChunk *subchunk = chunk->getSubChunk(subChunkIndex);
	if (!subchunk)
		return AIR;
	int localX = (worldPos.x % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;
	int localY = (worldPos.y % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;
	int localZ = (worldPos.z % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;
	return subchunk->getBlock({localX, localY, localZ});
}

void World::setRunning(std::mutex *runningMutex, bool *isRunning)
{
	_isRunning = isRunning;
	_runningMutex = runningMutex;
}

bool World::getIsRunning()
{
	std::lock_guard<std::mutex> lockGuard(*_runningMutex);
	return *_isRunning;
}

void World::unloadChunk()
{
	//TODO Save or do not unload modified chunks (delete block)
	//(Add a isModified boolean in Chunk or SubChunk class)
	_chunksListMutex.lock();
	size_t chunksNb = _chunkList.size();
	_chunksListMutex.unlock();

	if (chunksNb <= CACHE_SIZE)
		return ;

	// Get player position in chunk coordinates (true floor division)
	vec3 playerPos = _camera.getWorldPosition();
	int playerChunkX = static_cast<int>(std::floor(playerPos.x / static_cast<float>(CHUNK_SIZE)));
	int playerChunkZ = static_cast<int>(std::floor(playerPos.z / static_cast<float>(CHUNK_SIZE)));

	// Find the farthest chunk
	_chunksListMutex.lock();
	auto farthestChunkIt = _chunkList.end();
	float maxDistance = -1.0f;
	for (auto it = _chunkList.begin(); it != _chunkList.end(); ++it)
	{
		Chunk* chunk = *it;
		int chunkX = chunk->getPosition().x;
		int chunkZ = chunk->getPosition().y;
		float distance = sqrt(pow(chunkX - playerChunkX, 2) + pow(chunkZ - playerChunkZ, 2));
		if (distance > maxDistance)
		{
			maxDistance = distance;
			farthestChunkIt = it;
		}
	}

	if (farthestChunkIt != _chunkList.end())
	{
		Chunk* chunkToRemove = *farthestChunkIt;
		// Remove from _chunkList
		_chunkList.erase(farthestChunkIt);
		_chunksListMutex.unlock();
		
		ivec2 pos = chunkToRemove->getPosition();
		
		// Check if chunk is being displayed
		_displayedChunksMutex.lock();
		bool isDisplayed = false;
		auto endDisplay = _displayedChunks.end();
		auto displayIt = _displayedChunks.find(pos);
		isDisplayed = endDisplay != displayIt;
		_displayedChunksMutex.unlock();
		if (isDisplayed)
			return ;

		// Remove from _chunks
		_chunksMutex.lock();
		_chunks.erase(pos);
		_chunksMutex.unlock();

		// Clean up memory
		_perlinGenerator.removePerlinMap(pos.x, pos.y);
		chunkToRemove->unloadNeighbors();
		chunkToRemove->freeSubChunks();
		delete chunkToRemove;
	}
	else
	{
		_chunksListMutex.unlock();
	}
}

Chunk *World::loadChunk(int x, int z, int render, ivec2 &chunkPos, int resolution)
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
	}
	else
	{
		PerlinMap *pMap = _perlinGenerator.getPerlinMap(pos, resolution);
		Chunk *newChunk = new Chunk(pos, pMap, _caveGen, *this, _textureManager, _threadPool, resolution);

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

			_memorySize += chunk->getMemorySize();
			{
				std::lock_guard<std::mutex> lk(_chunksListMutex);
				_chunkList.emplace_back(chunk);
			}
		}
	}

	{
		std::lock_guard<std::mutex> lk(_displayedChunksMutex);
		_displayedChunks[pos] = chunk;
	}
	// unloadChunk();
	return chunk;
}

void World::loadTopChunks(int render, ivec2 &chunkPos, int resolution)
{
	int z = 0;
	for (int x = 0; x < render && getIsRunning(); x++)
	{
		loadChunk(x, z, render, chunkPos, resolution);
	}
}

void World::loadBotChunks(int render, ivec2 &chunkPos, int resolution)
{
	int z = render - 1;
	for (int x = render - 1; getIsRunning() && x >= 0; x--)
	{
		loadChunk(x, z, render, chunkPos, resolution);
	}
}

void World::loadRightChunks(int render, ivec2 &chunkPos, int resolution)
{
	int x = render - 1;
	for (int z = 1; z < render - 1 && getIsRunning(); z++) // avoid corners
	{
		loadChunk(x, z, render, chunkPos, resolution);
	}
}

void World::loadLeftChunks(int render, ivec2 &chunkPos, int resolution)
{
	int x = 0;
	for (int z = render - 2; getIsRunning() && z > 0; z--) // avoid corners
	{
		loadChunk(x, z, render, chunkPos, resolution);
	}
}

void World::loadFirstChunks(ivec2 &chunkPos)
{
	int renderDistance = _renderDistance;

	int resolution = RESOLUTION;
	_threshold = LOD_THRESHOLD;
	// _chronoHelper.startChrono(1, "Build chunks + loaded faces");
	// _chronoHelper.stopChrono(1);
	// _chronoHelper.printChrono(1);
	std::vector<std::future<void>> retLst;
	_currentRender = 0;
	for (int render = 0; getIsRunning() && render < renderDistance; render += 2)
	{
		std::future<void> retTop;
		std::future<void> retBot;
		std::future<void> retRight;
		std::future<void> retLeft;

		// Load chunks
		retTop = _threadPool.enqueue(&World::loadTopChunks, this, render, chunkPos, resolution);
		retBot = _threadPool.enqueue(&World::loadRightChunks, this, render, chunkPos, resolution);
		retRight = _threadPool.enqueue(&World::loadBotChunks, this, render, chunkPos, resolution);
		retLeft = _threadPool.enqueue(&World::loadLeftChunks, this, render, chunkPos, resolution);

		retTop.get();
		retBot.get();
		retRight.get();
		retLeft.get();

		if (render >= _threshold && resolution < CHUNK_SIZE)
		{
			resolution *= 2;
			_threshold = _threshold * 2;
		}
		retLst.emplace_back(_threadPool.enqueue(&World::updateFillData, this));
		if (hasMoved(chunkPos))
			break;
		_currentRender = render + 2;
	}
	// updateFillData();

	for (std::future<void> &ret : retLst)
	{
		ret.get();
	}
	retLst.clear();
}

int *World::getCurrentRenderPtr() {
	return &_currentRender;
}

size_t *World::getMemorySizePtr() {
	return &_memorySize;
}

void World::unLoadNextChunks(ivec2 &newCamChunk)
{
	std::vector<ivec2> toErase;
	{
		std::lock_guard<std::mutex> lk(_displayedChunksMutex);
		for (auto &kv : _displayedChunks)
		{
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
			_displayedChunks.erase(key);
	}
	updateFillData();
}

void World::updateFillData()
{
	// Avoid piling up heavy builds and starving the render thread
	if (_buildingDisplay.exchange(true))
		return;
	// If a staged update already exists, skip this build (we will coalesce)
	{
		std::lock_guard<std::mutex> lk(_drawDataMutex);
		if (!_stagedDataQueue.empty() || !_transparentStagedDataQueue.empty())
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
		_stagedDataQueue.emplace(fillData);
		_transparentStagedDataQueue.emplace(transparentData);
	}
	_buildingDisplay = false;
}

bool World::hasMoved(ivec2 &oldPos)
{
	ivec2 camChunk = _camera.getChunkPosition(CHUNK_SIZE);

	if (((floor(oldPos.x) != floor(camChunk.x) || floor(oldPos.y) != floor(camChunk.y))))
		return true;
	return false;
}

SubChunk *World::getSubChunk(ivec3 &position)
{
	Chunk* c = nullptr;
	{
		std::lock_guard<std::mutex> lk(_chunksMutex);
		auto it = _chunks.find(ivec2(position.x, position.z));
		if (it != _chunks.end()) c = it->second;
	}
	if (c) return c->getSubChunk(position.y);
	return nullptr;
}

Chunk *World::getChunk(ivec2 &position)
{
	std::lock_guard<std::mutex> lk(_chunksMutex);
	auto it = _chunks.find({position.x, position.y});
	if (it != _chunks.end())
		return it->second;
	return nullptr;
}

// static void validateIndirectAgainstInstances(const std::vector<DrawArraysIndirectCommand>& cmds,
// 	size_t nInst, const char* tag)
// {
// 	for (size_t i = 0; i < cmds.size(); ++i) {
// 		const uint64_t end = (uint64_t)cmds[i].baseInstance + (uint64_t)cmds[i].instanceCount;
// 		if (end > nInst) {
// 			std::cerr << "[VALIDATE][" << tag << "] draw " << i
// 			<< " overflows instances: base=" << cmds[i].baseInstance
// 			<< " count=" << cmds[i].instanceCount
// 			<< " nInst=" << nInst << std::endl;
// 			break;
// 		}
// 	}
// }

void World::buildFacesToDisplay(DisplayData* fillData, DisplayData* transparentFillData)
{
	std::vector<std::pair<ivec2, Chunk*>> items;
	items.reserve(_displayedChunks.size());
	{
		_displayedChunksMutex.lock();
		for (auto& kv : _displayedChunks) items.emplace_back(kv.first, kv.second);
		_displayedChunksMutex.unlock();
	}
	std::sort(items.begin(), items.end(), [](auto& a, auto& b){
		if (a.first.x != b.first.x) return a.first.x < b.first.x;
		return a.first.y < b.first.y;
	});

	for (auto& kv : items)
	{
		Chunk* c = kv.second;

		// --- immutable snapshot from the chunk ---
		std::vector<int> sv, tv;
		std::vector<DrawArraysIndirectCommand> ic, tic;
		std::vector<vec4> ssbo;
		if (!c)
			continue ;
		c->snapshotDisplayData(sv, ic, ssbo, tv, tic);

		// ---------- SOLID ----------
		const uint32_t startSolid = (uint32_t)fillData->indirectBufferData.size();
		const size_t   instBefore = fillData->vertexData.size();

		fillData->vertexData.insert(fillData->vertexData.end(), sv.begin(), sv.end());

		for (size_t i = 0; i < ic.size(); ++i) {
			auto cmd = ic[i];
			cmd.baseInstance += (GLuint)instBefore;   // baseInstance is index into packed inst[] SSBO
			fillData->indirectBufferData.push_back(cmd);

			glm::vec3 o = glm::vec3(ssbo[i].x, ssbo[i].y, ssbo[i].z);
			fillData->cmdAABBsSolid.push_back({ o, o + glm::vec3(CHUNK_SIZE) });
		}

		fillData->ssboData.insert(fillData->ssboData.end(), ssbo.begin(), ssbo.end());
		fillData->chunkCmdRanges[c->getPosition()] = {
			startSolid, (uint32_t)(fillData->indirectBufferData.size() - startSolid)
		};

		// ---------- TRANSPARENT ----------
		const uint32_t startT  = (uint32_t)transparentFillData->indirectBufferData.size();
		const size_t   tBefore = transparentFillData->vertexData.size();

		transparentFillData->vertexData.insert(transparentFillData->vertexData.end(), tv.begin(), tv.end());

		for (size_t i = 0; i < tic.size(); ++i) {
			auto cmd = tic[i];
			cmd.baseInstance += (GLuint)tBefore;
			transparentFillData->indirectBufferData.push_back(cmd);

			glm::vec3 o = glm::vec3(ssbo[i].x, ssbo[i].y, ssbo[i].z);
			transparentFillData->cmdAABBsTransp.push_back({ o, o + glm::vec3(CHUNK_SIZE) });
		}

		transparentFillData->ssboData.insert(transparentFillData->ssboData.end(), ssbo.begin(), ssbo.end());

		transparentFillData->chunkCmdRanges[c->getPosition()] = {
			startT, (uint32_t)(transparentFillData->indirectBufferData.size() - startT)
		};
	}

	if (fillData->indirectBufferData.size() != fillData->ssboData.size()) {
		std::cout << "[BUILD] Solid mismatch draws=" << fillData->indirectBufferData.size()
					<< " ssbo=" << fillData->ssboData.size() << std::endl;
	}
	if (transparentFillData->indirectBufferData.size() != transparentFillData->ssboData.size()) {
		std::cout << "[BUILD] Transp mismatch draws=" << transparentFillData->indirectBufferData.size()
					<< " ssbo=" << transparentFillData->ssboData.size() << std::endl;
	}
}

void World::updateDrawData()
{
	std::lock_guard<std::mutex> lock(_drawDataMutex);
	// queues: keep only the newest staged data to minimize uploads
	while (_stagedDataQueue.size() > 1)
	{
		DisplayData *old = _stagedDataQueue.front();
		_stagedDataQueue.pop();
		delete old;
	}
	while (_transparentStagedDataQueue.size() > 1)
	{
		DisplayData *old = _transparentStagedDataQueue.front();
		_transparentStagedDataQueue.pop();
		delete old;
	}
	if (!_stagedDataQueue.empty())
	{
		DisplayData *stagedData = _stagedDataQueue.front();
		std::swap(stagedData, _drawData);
		delete stagedData;
		stagedData = nullptr;
		_stagedDataQueue.pop();
		_needUpdate = true;
	}
	if (!_transparentStagedDataQueue.empty())
	{
		
		DisplayData *stagedData = _transparentStagedDataQueue.front();
		std::swap(stagedData, _transparentDrawData);
		delete stagedData;
		stagedData = nullptr;
		_transparentStagedDataQueue.pop();
		_needTransparentUpdate = true;
	}
	// SSBOs are uploaded per-pass in pushVerticesToOpenGL
	if (_drawData) {
		if (_drawData->ssboData.size() != _drawData->indirectBufferData.size()) {
			std::cout << "[CULL] Mismatch solid: ssbo=" << _drawData->ssboData.size()
					<< " cmds=" << _drawData->indirectBufferData.size() << std::endl;
		}
	}
	// Transparent sanity (uses the same SSBO)
	if (_transparentDrawData) {
		if (_drawData && _drawData->ssboData.size() != _transparentDrawData->indirectBufferData.size()) {
			std::cout << "[CULL] Mismatch transp: ssbo=" << _drawData->ssboData.size()
					<< " cmds=" << _transparentDrawData->indirectBufferData.size() << std::endl;
		}
	}
}
int World::display()
{
	if (!_drawData) return 0;
	if (_needUpdate) { pushVerticesToOpenGL(false); }
	if (_solidDrawCount == 0) return 0;

	runGpuCulling(false);

	// Bind per-pass SSBOs (single-buffer path)
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, _solidPosSSBO);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, _solidInstSSBO);

	glBindVertexArray(_vao);
	glBindBuffer(GL_DRAW_INDIRECT_BUFFER, _indirectBuffer);
	glMultiDrawArraysIndirect(GL_TRIANGLE_STRIP, nullptr, _solidDrawCount, 0);
	glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
	glBindVertexArray(0);

	long long tris = 0;
	for (const auto& c : _drawData->indirectBufferData) {
		long long per = (c.count >= 3) ? (c.count - 2) : 0;
		tris += (long long)c.instanceCount * per;
	}
	return (int)tris;
}

int World::displayTransparent()
{
	if (!_transparentDrawData) return 0;
	if (_needTransparentUpdate) { pushVerticesToOpenGL(true); }
	if (_transpDrawCount == 0) return 0;

	runGpuCulling(true);

	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, _transpPosSSBO);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, _transpInstSSBO);

	glDisable(GL_CULL_FACE);
	glBindVertexArray(_transparentVao);
	glBindBuffer(GL_DRAW_INDIRECT_BUFFER, _transparentIndirectBuffer);
	glMultiDrawArraysIndirect(GL_TRIANGLE_STRIP, nullptr, _transpDrawCount, 0);
	glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
	glBindVertexArray(0);

	return (int)_transpDrawCount;
}

void World::pushVerticesToOpenGL(bool transparent)
{
	std::lock_guard<std::mutex> lock(_drawDataMutex);

	// Helper to grow buffers once and update with SubData (reduces realloc waiting)
	auto ensureCapacityAndUpload = [](GLuint buf, GLsizeiptr& cap, GLsizeiptr neededBytes, const void* data, GLenum usage)
	{
		if (cap < neededBytes)
		{
			GLsizeiptr newCap = cap > 0 ? cap : (GLsizeiptr)4096;
			while (newCap < neededBytes) newCap *= 2;
			glNamedBufferData(buf, newCap, nullptr, usage);
			cap = newCap;
		}
		if (neededBytes > 0)
			glNamedBufferSubData(buf, 0, neededBytes, data);
	};

	if (transparent)
	{
		const GLsizeiptr nCmd      = (GLsizeiptr)_transparentDrawData->indirectBufferData.size();
		const GLsizeiptr bytesCmd  = nCmd * sizeof(DrawArraysIndirectCommand);
		const GLsizeiptr nInst     = (GLsizeiptr)_transparentDrawData->vertexData.size();
		const GLsizeiptr bytesInst = nInst * sizeof(int);
		const GLsizeiptr bytesSSBO = (GLsizeiptr)_drawData->ssboData.size() * sizeof(glm::vec4);

		// Ensure buffers exist
		if (!_transpInstSSBO) glCreateBuffers(1, &_transpInstSSBO);
		if (!_transpPosSSBO)  glCreateBuffers(1, &_transpPosSSBO);

		// Upload template and out buffers (compute read/write) with capacity growth
		ensureCapacityAndUpload(_transpTemplIndirectBuffer, _capTemplTranspCmd, bytesCmd,
			_transparentDrawData->indirectBufferData.data(), GL_STATIC_DRAW);
		ensureCapacityAndUpload(_transparentIndirectBuffer, _capOutTranspCmd, bytesCmd,
			_transparentDrawData->indirectBufferData.data(), GL_DYNAMIC_DRAW);

		// Upload instance SSBO (binding=4)
		ensureCapacityAndUpload(_transpInstSSBO, _capTranspInst, bytesInst,
			_transparentDrawData->vertexData.data(), GL_DYNAMIC_DRAW);

		// Upload pos/res SSBO (binding=3). Share capacity with solid path via _capSSBO
		ensureCapacityAndUpload(_transpPosSSBO, _capTranspSSBO, bytesSSBO,
			_drawData->ssboData.data(), GL_DYNAMIC_DRAW);

		_transpDrawCount = (GLsizei)nCmd;
		_needTransparentUpdate = false;
	}
	else
	{
		const GLsizeiptr nCmd      = (GLsizeiptr)_drawData->indirectBufferData.size();
		const GLsizeiptr bytesCmd  = nCmd * sizeof(DrawArraysIndirectCommand);
		const GLsizeiptr nInst     = (GLsizeiptr)_drawData->vertexData.size();
		const GLsizeiptr bytesInst = nInst * sizeof(int);
		const GLsizeiptr bytesSSBO = (GLsizeiptr)_drawData->ssboData.size() * sizeof(glm::vec4);

		// Ensure buffers exist
		if (!_solidInstSSBO) glCreateBuffers(1, &_solidInstSSBO);
		if (!_solidPosSSBO)  glCreateBuffers(1, &_solidPosSSBO);

		// Upload template and out buffers (compute read/write) with capacity growth
		ensureCapacityAndUpload(_templIndirectBuffer, _capTemplSolidCmd, bytesCmd,
			_drawData->indirectBufferData.data(), GL_STATIC_DRAW);
		ensureCapacityAndUpload(_indirectBuffer, _capOutSolidCmd, bytesCmd,
			_drawData->indirectBufferData.data(), GL_DYNAMIC_DRAW);

		// Upload instance SSBO (binding=4)
		ensureCapacityAndUpload(_solidInstSSBO, _capSolidInst, bytesInst,
			_drawData->vertexData.data(), GL_DYNAMIC_DRAW);

		// Upload pos/res SSBO (binding=3)
		ensureCapacityAndUpload(_solidPosSSBO, _capSolidSSBO, bytesSSBO,
			_drawData->ssboData.data(), GL_DYNAMIC_DRAW);

		_solidDrawCount = (GLsizei)nCmd;
		_needUpdate = false;
	}
}

void World::initGpuCulling() {
	_cullProgram  = compileComputeShader("shaders/compute/frustum_cull.glsl");
	_locNumDraws  = glGetUniformLocation(_cullProgram, "numDraws");
	_locChunkSize = glGetUniformLocation(_cullProgram, "chunkSize");

	// Frustum UBO (6 vec4)
	glCreateBuffers(1, &_frustumUBO);
	glNamedBufferData(_frustumUBO, sizeof(glm::vec4) * 6, nullptr, GL_DYNAMIC_DRAW);
}
void World::initGLBuffer()
{
	if (_hasBufferInitialized) return;

	glGenVertexArrays(1, &_vao);
	glGenBuffers(1, &_vbo);
	glGenBuffers(1, &_indirectBuffer);

	// Static quad strip
	GLint vertices[] = { 0,0,0, 1,0,0, 0,1,0, 1,1,0 };

	// Solid VAO
	glBindVertexArray(_vao);
	glBindBuffer(GL_ARRAY_BUFFER, _vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
	glVertexAttribIPointer(0, 3, GL_INT, 3 * sizeof(GLint), (void*)0);
	glEnableVertexAttribArray(0);
	glBindVertexArray(0);

	// Transparent VAO
	glGenVertexArrays(1, &_transparentVao);
	glGenBuffers(1, &_transparentIndirectBuffer);

	glBindVertexArray(_transparentVao);
	glBindBuffer(GL_ARRAY_BUFFER, _vbo);
	glVertexAttribIPointer(0, 3, GL_INT, 3 * sizeof(GLint), (void*)0);
	glEnableVertexAttribArray(0);
	glBindVertexArray(0);

	// Create template SSBOs for compute inputs
	glCreateBuffers(1, &_templIndirectBuffer);
	glCreateBuffers(1, &_transpTemplIndirectBuffer);

	initGpuCulling();
	_hasBufferInitialized = true;
}


void World::updatePerlinMapResolution(PerlinMap *pMap, int newResolution)
{
	_perlinGenerator.updatePerlinMapResolution(pMap, newResolution);
}

void World::setViewProj(const glm::mat4& view, const glm::mat4& proj) {
	// Build 6 normalized planes from VP
	Frustum f = Frustum::fromVP(proj * view);

	glm::vec4 planes[6];
	for (int i = 0; i < 6; ++i)
	{
		planes[i] = glm::vec4(f.p[i].n, f.p[i].d);
	}
	// Update UBO
	glNamedBufferSubData(_frustumUBO, 0, sizeof(planes), planes);
}

void World::runGpuCulling(bool transparent)
{
	GLuint templ = transparent ? _transpTemplIndirectBuffer : _templIndirectBuffer;
	GLuint out   = transparent ? _transparentIndirectBuffer : _indirectBuffer;

	const GLsizei count = transparent ? _transpDrawCount : _solidDrawCount;
	if (count <= 0) return;

	// ensure previous uploads visible to compute
	glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);

	GLint prevProg = 0; glGetIntegerv(GL_CURRENT_PROGRAM, &prevProg);
	glUseProgram(_cullProgram);

	// Bind pass' pos/res buffer
	GLuint posBuf = transparent ? _transpPosSSBO : _solidPosSSBO;
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, posBuf);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, templ);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, out);
	glBindBufferBase(GL_UNIFORM_BUFFER,        3, _frustumUBO);

	glUniform1ui(_locNumDraws, (GLuint)count);
	glUniform1f (_locChunkSize, (float)CHUNK_SIZE);

	const GLuint groups = ((GLuint)count + 63u) / 64u;
	glDispatchCompute(groups, 1, 1);

	glMemoryBarrier(GL_COMMAND_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT);
	glUseProgram(prevProg ? (GLuint)prevProg : 0);
}
