#include "World.hpp"

World::World(int seed, TextureManager &textureManager, Camera &camera, ThreadPool &pool) : _threadPool(pool), _textureManager(textureManager), _camera(camera), _perlinGenerator(seed), _caveGen(1000, 0.01f, 0.05f, 0.6f, 0.6f, 42)
{
	_needUpdate = true;
	_needTransparentUpdate = true;
	_hasBufferInitialized = false;
	_renderDistance = RENDER_DISTANCE;
	_drawnSSBOSize = 1;
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
	camera.setPos({camPos.x, -(topBlock.height+3), camPos.z});
}

TopBlock World::findTopBlockY(ivec2 chunkPos, ivec2 worldPos) {
    auto chunk = getChunkShared(chunkPos);
    if (!chunk) return TopBlock();
    int localX = (worldPos.x % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;
    int localZ = (worldPos.y % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;
    return chunk->getTopBlock(localX, localZ);
}

TopBlock World::findBlockUnderPlayer(ivec2 chunkPos, ivec3 worldPos) {
    auto chunk = getChunkShared(chunkPos);
    if (!chunk) return TopBlock();
    int localX = (worldPos.x % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;
    int localY = (worldPos.y % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;
    int localZ = (worldPos.z % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;
    return chunk->getTopBlockUnderPlayer(localX, localY, localZ);
}

void World::init(int renderDistance = RENDER_DISTANCE) {
	_renderDistance = renderDistance;
	initGLBuffer();
}

World::~World()
{
    // stopWorkers(); // if you have one, ensure threads are joined first

    {
        std::lock_guard<std::mutex> lock(_chunksMutex);
        _chunks.clear();            // drops refs; Chunks free themselves
    }
    {
        std::lock_guard<std::mutex> lk(_chunksListMutex);
        _chunkList.clear();
    }
    {
        std::lock_guard<std::mutex> lk(_displayedChunksMutex);
        _displayedChunks.clear();
    }

    while (!_stagedDataQueue.empty()) {
        auto* tmp = _stagedDataQueue.front(); _stagedDataQueue.pop();
        delete tmp;
    }
    while (!_transparentStagedDataQueue.empty()) {
        auto* tmp = _transparentStagedDataQueue.front(); _transparentStagedDataQueue.pop();
        delete tmp;
    }
    delete _drawData;
    delete _transparentDrawData;
    delete _transparentFillData;
    delete _transparentStagingData;
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
	auto chunk = getChunkShared(chunkPos);
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

bool World::setBlock(ivec2 chunkPos, ivec3 worldPos, BlockType value)
{
    auto chunk = getChunkShared(chunkPos);
    if (!chunk) return false;

    const int subY = (int)std::floor((double)worldPos.y / (double)CHUNK_SIZE);
    SubChunk* sc = chunk->getOrCreateSubChunk(subY, /*generate=*/false);
    if (!sc) return false;

    const int lx = (worldPos.x % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;
    const int ly = (worldPos.y % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;
    const int lz = (worldPos.z % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;

    sc->setBlock(lx, ly, lz, value);

    // mark this chunk for a one-time remesh
    markChunkDirty(chunkPos);
    return true;
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
    // keep most-recent CACHE_SIZE in memory
    size_t count;
    { std::lock_guard<std::mutex> lk(_chunksListMutex); count = _chunkList.size(); }
    if (count <= CACHE_SIZE) return;

    // player chunk coords
    ivec3 p = _camera.getWorldPosition();
    int pcx = p.x / CHUNK_SIZE; if (p.x < 0) --pcx;
    int pcz = p.z / CHUNK_SIZE; if (p.z < 0) --pcz;

    // pick farthest candidate (from the list)
    std::shared_ptr<Chunk> victim;
    ivec2 pos;
    {
        std::lock_guard<std::mutex> lk(_chunksListMutex);

        float maxDist = -1.f;
        auto farIt = _chunkList.end();

        for (auto it = _chunkList.begin(); it != _chunkList.end(); ++it) {
            if (!*it) { it = _chunkList.erase(it); if (it == _chunkList.end()) break; continue; }
            const ivec2 cpos = (*it)->getPosition();
            float dx = float(cpos.x - pcx), dz = float(cpos.y - pcz);
            float d  = std::sqrt(dx*dx + dz*dz);
            if (d > maxDist) { maxDist = d; farIt = it; }
        }

        if (farIt == _chunkList.end()) return;

        victim = *farIt;
        pos    = victim->getPosition();
        _chunkList.erase(farIt);           // temporarily remove from LRU list
    }

    // if still displayed, put back and bail
    {
        std::lock_guard<std::mutex> lk(_displayedChunksMutex);
        if (_displayedChunks.count(pos)) {
            std::lock_guard<std::mutex> lk2(_chunksListMutex);
            _chunkList.emplace_back(victim);  // reinsert
            return;
        }
        _displayedChunks.erase(pos); // ensure it is not marked displayed
    }

    // erase from the main map (other threads holding shared_ptr keep it alive)
    {
        std::lock_guard<std::mutex> lk(_chunksMutex);
        _chunks.erase(pos);
    }

    // book-keeping (memory size will actually drop when last ref releases)
    _memorySize -= victim->getMemorySize();

    // Defer heavy cleanup to Chunk dtor (recommended).
    // If you *must* free perlin now, do it only when no other refs exist:
    if (victim.use_count() == 1) {
        _perlinGenerator.removePerlinMap(pos.x, pos.y);
        // nothing else to do; shared_ptr will destroy now
    }
    // else: let the last holder trigger the destructor later.
}


Chunk* World::loadChunk(int x, int z, int render, ivec2& chunkPos, int resolution)
{
    auto sp = loadChunkShared(x, z, render, chunkPos, resolution);
    return sp.get();
}

std::shared_ptr<Chunk> World::loadChunkShared(int x, int z, int render, ivec2& chunkPos, int resolution)
{
    ivec2 pos = { chunkPos.x - render / 2 + x, chunkPos.y - render / 2 + z };

    // fast path: try find
    std::shared_ptr<Chunk> chunk;
    {
        std::lock_guard<std::mutex> lock(_chunksMutex);
        auto it = _chunks.find(pos);
        if (it != _chunks.end())
            chunk = it->second;
    }

    bool created = false;
    if (!chunk)
    {
        // heavy work outside the map lock
        PerlinMap* pMap = _perlinGenerator.getPerlinMap(pos, resolution);
        auto newChunk = std::make_shared<Chunk>(pos, pMap, _caveGen, *this, _textureManager, _threadPool, resolution);

        // insert (second check to avoid duplicate creators)
        {
            std::lock_guard<std::mutex> lock(_chunksMutex);
            auto [it, inserted] = _chunks.emplace(pos, newChunk);
            if (inserted) { chunk = std::move(newChunk); created = true; }
            else          { chunk = it->second; }
        }

        if (created) {
            chunk->loadBlocks();
            chunk->getNeighbors();
            _memorySize += chunk->getMemorySize();
            {
                std::lock_guard<std::mutex> lk(_chunksListMutex);
                _chunkList.emplace_back(chunk);
            }
			chunk->sendFacesToDisplay(); 
			applyPendingFor(pos);
        }
    }

    // resolution downgrades are cheap; do outside locks
    if (chunk && chunk->getResolution() > resolution)
        chunk->updateResolution(resolution);

    // mark as displayed
    if (chunk) {
        std::lock_guard<std::mutex> lock(_displayedChunksMutex);
        _displayedChunks[pos] = chunk;
    }

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
    // collect positions to erase (snapshot under lock)
    std::vector<ivec2> toErase;
    {
        std::lock_guard<std::mutex> lk(_displayedChunksMutex);
        toErase.reserve(_displayedChunks.size());
        for (const auto &kv : _displayedChunks) {
            const std::shared_ptr<Chunk>& chunk = kv.second;
            const ivec2 cpos = chunk->getPosition();
            if (std::abs(cpos.x - newCamChunk.x) > _renderDistance / 2 ||
                std::abs(cpos.y - newCamChunk.y) > _renderDistance / 2) {
                toErase.emplace_back(cpos);
            }
        }
        // erase while we still hold the lock so readers see a consistent map
        for (const auto &pos : toErase) {
            _displayedChunks.erase(pos);
        }
    }

    // rebuild GPU buffers after we changed the displayed set
    updateFillData();
}

void World::updateFillData()
{
    // Make sure any cross-chunk edits are reflected in meshes
    flushDirtyChunks();

    DisplayData* fillData        = new DisplayData();
    DisplayData* transparentData = new DisplayData();
    buildFacesToDisplay(fillData, transparentData);
    std::lock_guard<std::mutex> g(_drawDataMutex);
    _stagedDataQueue.emplace(fillData);
    _transparentStagedDataQueue.emplace(transparentData);
}

bool World::hasMoved(ivec2 &oldPos)
{
	ivec2 camChunk = _camera.getChunkPosition(CHUNK_SIZE);

	if (((floor(oldPos.x) != floor(camChunk.x) || floor(oldPos.y) != floor(camChunk.y))))
		return true;
	return false;
}

SubChunk* World::getSubChunk(ivec3 &position)
{
    std::shared_ptr<Chunk> c;
    {
        std::lock_guard<std::mutex> lk(_chunksMutex);
        auto it = _chunks.find(ivec2(position.x, position.z));
        if (it != _chunks.end()) c = it->second;
    }
    return c ? c->getSubChunk(position.y) : nullptr;
}

std::shared_ptr<Chunk> World::getChunkShared(const ivec2& pos) {
    std::lock_guard<std::mutex> lock(_chunksMutex);
    auto it = _chunks.find(pos);
    if (it == _chunks.end()) return {};
    return it->second; // copy shared_ptr (increments refcount)
}

void World::buildFacesToDisplay(DisplayData* fillData, DisplayData* transparentFillData)
{
    // snapshot displayed chunks (avoid holding the lock during heavy copies)
    std::vector<std::shared_ptr<Chunk>> snapshot;
    {
        std::lock_guard<std::mutex> lk(_displayedChunksMutex);
        snapshot.reserve(_displayedChunks.size());
        for (auto &kv : _displayedChunks) snapshot.push_back(kv.second);
    }

    for (const auto& c : snapshot) {
        // SSBO order must match indirect command order inside the chunk
        auto& ssbo = c->getSSBO();

        // --- SOLID ---
        uint32_t startSolid = (uint32_t)fillData->indirectBufferData.size();

        auto& solidVerts = c->getVertices();
        size_t vSizeBefore = fillData->vertexData.size();
        fillData->vertexData.insert(fillData->vertexData.end(), solidVerts.begin(), solidVerts.end());

        auto& ib = c->getIndirectData();
        size_t solidCount = ib.size();
        size_t ssboLimit  = std::min(ssbo.size(), solidCount);
        for (size_t i = 0; i < solidCount; ++i) {
            auto cmd = ib[i];
            cmd.baseInstance += (uint32_t)vSizeBefore;
            fillData->indirectBufferData.push_back(cmd);

            // AABB from SSBO[i] (guard if ssbo is shorter)
            size_t idx = std::min(i, ssboLimit ? ssboLimit - 1 : (size_t)0);
            glm::vec3 o = glm::vec3(ssbo[idx].x, ssbo[idx].y, ssbo[idx].z);
            fillData->cmdAABBsSolid.push_back({ o, o + glm::vec3(CHUNK_SIZE) });
        }

        fillData->ssboData.insert(fillData->ssboData.end(), ssbo.begin(), ssbo.end());
        fillData->chunkCmdRanges[c->getPosition()] = {
            startSolid, (uint32_t)(fillData->indirectBufferData.size() - startSolid)
        };

        // --- TRANSPARENT ---
        uint32_t startT = (uint32_t)transparentFillData->indirectBufferData.size();

        auto& transpVerts = c->getTransparentVertices();
        size_t tvBefore = transparentFillData->vertexData.size();
        transparentFillData->vertexData.insert(
            transparentFillData->vertexData.end(), transpVerts.begin(), transpVerts.end()
        );

        auto& tib = c->getTransparentIndirectData();
        size_t transpCount = tib.size();
        size_t ssboLimitT  = std::min(ssbo.size(), transpCount);
        for (size_t i = 0; i < transpCount; ++i) {
            auto cmd = tib[i];
            cmd.baseInstance += (uint32_t)tvBefore;
            transparentFillData->indirectBufferData.push_back(cmd);

            size_t idx = std::min(i, ssboLimitT ? ssboLimitT - 1 : (size_t)0);
            glm::vec3 o = glm::vec3(ssbo[idx].x, ssbo[idx].y, ssbo[idx].z);
            transparentFillData->cmdAABBsTransp.push_back({ o, o + glm::vec3(CHUNK_SIZE) });
        }

        transparentFillData->chunkCmdRanges[c->getPosition()] = {
            startT, (uint32_t)(transparentFillData->indirectBufferData.size() - startT)
        };
    }
}


void World::updateSSBO()
{
	// SSBO Update
	bool needUpdate = false;
	size_t size = _drawData->ssboData.size() * 2;
	while (size > _drawnSSBOSize)
	{
		_drawnSSBOSize *= 2;
		needUpdate = true;
	}
	if (needUpdate)
	{
		glDeleteBuffers(1, &_ssbo);
		glCreateBuffers(1, &_ssbo);
		glNamedBufferStorage(_ssbo, 
			sizeof(vec4) * _drawnSSBOSize, 
			nullptr, 
			GL_DYNAMIC_STORAGE_BIT);
		glNamedBufferSubData(
			_ssbo,
			0,
			sizeof(vec4) * _drawData->ssboData.size(),
			_drawData->ssboData.data()
		);
	}
}

void World::updateDrawData()
{
    std::lock_guard<std::mutex> lock(_drawDataMutex);
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
	if (_drawData && (_needUpdate || _needTransparentUpdate))
		updateSSBO();
}

int World::display()
{
    if (!_drawData) return 0;
    if (_needUpdate) { pushVerticesToOpenGL(false); _needUpdate = false; }

    // Mask indirect commands for this frame
    applyFrustumCulling(false);

    // === triangles actually drawn (after culling) ===
    const auto& cmds = _frustumValid ? _culledSolidCmds
                                     : _drawData->indirectBufferData;
    long long triangles = 0;
    for (const auto& c : cmds) {
        long long per = (c.count >= 3) ? (c.count - 2) : 0; // strip: 4 -> 2
        triangles += (long long)c.instanceCount * per;
    }

    if (cmds.empty()) return 0;

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, _ssbo);
    glBindVertexArray(_vao);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, _indirectBuffer);
    glMultiDrawArraysIndirect(GL_TRIANGLE_STRIP, nullptr, cmds.size(), 0);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
    glBindVertexArray(0);
    return (int)triangles;
}

int World::displayTransparent()
{
    if (!_transparentDrawData) return 0;

    if (_needTransparentUpdate) {
        pushVerticesToOpenGL(true);
        _needTransparentUpdate = false;
    }

    applyFrustumCulling(true);

    const auto& cmds = _frustumValid ? _culledTranspCmds
                                     : _transparentDrawData->indirectBufferData;
    long long triangles = 0;
    for (const auto& c : cmds) {
        long long per = (c.count >= 3) ? (c.count - 2) : 0;
        triangles += (long long)c.instanceCount * per;
    }

    if (cmds.empty()) return 0;

    glDisable(GL_CULL_FACE);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, _ssbo);
    glBindVertexArray(_transparentVao);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, _transparentIndirectBuffer);
    glMultiDrawArraysIndirect(GL_TRIANGLE_STRIP, nullptr, cmds.size(), 0);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
    glBindVertexArray(0);
    return (int)triangles;
}

void World::pushVerticesToOpenGL(bool isTransparent)
{
	if (isTransparent)
	{
		glBindBuffer(GL_DRAW_INDIRECT_BUFFER, _transparentIndirectBuffer);
		GLsizeiptr tisz = (GLsizeiptr)(sizeof(DrawArraysIndirectCommand) * _transparentDrawData->indirectBufferData.size());
		glBufferData(GL_DRAW_INDIRECT_BUFFER, tisz, nullptr, GL_STREAM_DRAW);
		glBufferSubData(GL_DRAW_INDIRECT_BUFFER, 0, tisz, _transparentDrawData->indirectBufferData.data());
		glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);

		if (_drawData->ssboData.size() != 0) {
			glNamedBufferSubData(
				_ssbo,
				0,
				sizeof(vec4) * _drawData->ssboData.size(),
				_drawData->ssboData.data()
			);
		}
		glBindBuffer(GL_ARRAY_BUFFER, _transparentInstanceVBO);
		GLsizeiptr tvsz = (GLsizeiptr)(sizeof(int) * _transparentDrawData->vertexData.size());
		glBufferData(GL_ARRAY_BUFFER, tvsz, nullptr, GL_STREAM_DRAW);
		glBufferSubData(GL_ARRAY_BUFFER, 0, tvsz, _transparentDrawData->vertexData.data());
		_needTransparentUpdate = false;
	}
	else
	{
		glBindBuffer(GL_DRAW_INDIRECT_BUFFER, _indirectBuffer);
		GLsizeiptr isz = (GLsizeiptr)(sizeof(DrawArraysIndirectCommand) * _drawData->indirectBufferData.size());
		glBufferData(GL_DRAW_INDIRECT_BUFFER, isz, nullptr, GL_STREAM_DRAW);               // orphan
		glBufferSubData(GL_DRAW_INDIRECT_BUFFER, 0, isz, _drawData->indirectBufferData.data());
		glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);

		if (_drawData->ssboData.size() != 0) {
			glNamedBufferSubData(
				_ssbo,
				0,
				sizeof(vec4) * _drawData->ssboData.size(),
				_drawData->ssboData.data()
			);
		}
		glBindBuffer(GL_ARRAY_BUFFER, _instanceVBO);
		GLsizeiptr vsz = (GLsizeiptr)(sizeof(int) * _drawData->vertexData.size());
		glBufferData(GL_ARRAY_BUFFER, vsz, nullptr, GL_STREAM_DRAW);                       // orphan
		glBufferSubData(GL_ARRAY_BUFFER, 0, vsz, _drawData->vertexData.data());
		_needUpdate = false;
	}
}

// void World::clearFaces() {
// 	// Clear solid data
// 	_fillData->vertexData.clear();
// 	_fillData->indirectBufferData.clear();

// 	// Clear transparent data
// 	_transparentFillData->vertexData.clear();
// 	_transparentFillData->indirectBufferData.clear();

// 	// Clear common ssbo
// 	_fillData->ssboData.clear();
// }

void World::initGLBuffer()
{
	if (_hasBufferInitialized == true)
		return ;
	glGenVertexArrays(1, &_vao);
	glGenBuffers(1, &_vbo);
	glGenBuffers(1, &_instanceVBO);
	glGenBuffers(1, &_indirectBuffer);
	glCreateBuffers(1, &_ssbo);
	
	// Static vertex data
	GLint vertices[] = {
		0, 0, 0,
		1, 0, 0,
		0, 1, 0,
		1, 1, 0,
	};
	
	// Setup solid VAO
	glBindVertexArray(_vao);
	
	// Vertex positions
	glBindBuffer(GL_ARRAY_BUFFER, _vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
	glVertexAttribIPointer(0, 3, GL_INT, 3 * sizeof(GLint), (void*)0);
	glEnableVertexAttribArray(0);
	
	// Instance data
	glBindBuffer(GL_ARRAY_BUFFER, _instanceVBO);
	glVertexAttribIPointer(2, 1, GL_INT, sizeof(int), (void*)0);
	glEnableVertexAttribArray(2);
	glVertexAttribDivisor(2, 1);
	
	glBindVertexArray(0);
	
	// Setup transparent VAO
	glGenVertexArrays(1, &_transparentVao);
	glGenBuffers(1, &_transparentInstanceVBO);
	glGenBuffers(1, &_transparentIndirectBuffer);
	
	glBindVertexArray(_transparentVao);
	
	// Vertex positions
	glBindBuffer(GL_ARRAY_BUFFER, _vbo); // shared buffer, no need to re-upload
	glVertexAttribIPointer(0, 3, GL_INT, 3 * sizeof(GLint), (void*)0);
	glEnableVertexAttribArray(0);
	
	// Instance data
	glBindBuffer(GL_ARRAY_BUFFER, _transparentInstanceVBO);
	glVertexAttribIPointer(2, 1, GL_INT, sizeof(int), (void*)0);
	glEnableVertexAttribArray(2);
	glVertexAttribDivisor(2, 1);
	
	glBindVertexArray(0);
	_hasBufferInitialized = true;
}

void World::updatePerlinMapResolution(PerlinMap *pMap, int newResolution)
{
	_perlinGenerator.updatePerlinMapResolution(pMap, newResolution);
}

void World::setViewProj(const glm::mat4& view, const glm::mat4& proj) {
    _frustum = Frustum::fromVP(proj * view);
    _frustumValid = true;
}

void World::applyFrustumCulling(bool transparent) {
    if (!_frustumValid) return;

    if (transparent) {
        if (!_transparentDrawData) return;
        _culledTranspCmds = _transparentDrawData->indirectBufferData;

        const auto& boxes = _transparentDrawData->cmdAABBsTransp;
        for (size_t i = 0; i < _culledTranspCmds.size(); ++i) {
            const auto& b = boxes[i];
            if (!_frustum.aabbVisible(b.mn, b.mx))
                _culledTranspCmds[i].instanceCount = 0;
        }

        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, _transparentIndirectBuffer);
        glBufferSubData(GL_DRAW_INDIRECT_BUFFER, 0,
            (GLsizeiptr)(sizeof(DrawArraysIndirectCommand) * _culledTranspCmds.size()),
            _culledTranspCmds.data());
        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
    } else {
        if (!_drawData) return;
        _culledSolidCmds = _drawData->indirectBufferData;

        const auto& boxes = _drawData->cmdAABBsSolid;
        for (size_t i = 0; i < _culledSolidCmds.size(); ++i) {
            const auto& b = boxes[i];
            if (!_frustum.aabbVisible(b.mn, b.mx))
                _culledSolidCmds[i].instanceCount = 0;
        }

        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, _indirectBuffer);
        glBufferSubData(GL_DRAW_INDIRECT_BUFFER, 0,
            (GLsizeiptr)(sizeof(DrawArraysIndirectCommand) * _culledSolidCmds.size()),
            _culledSolidCmds.data());
        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
    }
}
bool World::setBlockOrQueue(ivec2 chunkPos, ivec3 worldPos, BlockType value)
{
    auto chunk = getChunkShared(chunkPos);
    if (!chunk) {
        std::lock_guard<std::mutex> lk(_pendingMutex);
        _pendingEdits[chunkPos].push_back({worldPos, value});
        return false; // queued
    }
    return setBlock(chunkPos, worldPos, value); // applies immediately
}

void World::applyPendingFor(const ivec2& pos)
{
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

    // Now the chunk exists; apply all queued edits
    for (const auto& e : edits)
        setBlock(pos, e.worldPos, e.value);

    // Optionally remesh once per chunk after the batch:
    if (auto c = getChunkShared(pos)) c->sendFacesToDisplay();
    updateFillData();
}

void World::markChunkDirty(const ivec2& pos) {
    std::lock_guard<std::mutex> lk(_dirtyMutex);
    _dirtyChunks.insert(pos);
}

void World::flushDirtyChunks() {
    std::vector<ivec2> toRemesh;
    {
        std::lock_guard<std::mutex> lk(_dirtyMutex);
        if (_dirtyChunks.empty()) return;
        toRemesh.assign(_dirtyChunks.begin(), _dirtyChunks.end());
        _dirtyChunks.clear();
    }
    // Rebuild meshes for the affected chunks
    for (const auto& p : toRemesh) {
        if (auto c = getChunkShared(p)) c->sendFacesToDisplay();
    }
}
