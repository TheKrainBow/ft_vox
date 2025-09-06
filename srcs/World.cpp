#include "World.hpp"
#include "define.hpp"
#include <algorithm>
#include <cmath>

static inline int mod_floor(int a, int b) {
    int r = a % b;
    return (r < 0) ? r + b : r;
}
static inline int floor_div(int a, int b) {
    int q = a / b, r = a % b;
    return (r && ((r < 0) != (b < 0))) ? (q - 1) : q;
}

World::World(int seed, TextureManager &textureManager, Camera &camera, ThreadPool &pool)
: _threadPool(pool)
, _textureManager(textureManager)
, _camera(camera)
, _perlinGenerator(seed)
, _caveGen(1000, 0.01f, 0.05f, 0.6f, 0.6f, 42)
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
    vec3 worldPos  = camera.getWorldPosition();
    loadChunk(0, 0, 0, chunkPos, 1);
    TopBlock topBlock = findTopBlockY(chunkPos, {worldPos.x, worldPos.z});
    const vec3 &camPos = camera.getPosition();
    // Place camera: feet on ground
    camera.setPos({camPos.x - 0.5, -(topBlock.height + 1 + EYE_HEIGHT), camPos.z - 0.5});
}

TopBlock World::findTopBlockY(ivec2 chunkPos, ivec2 worldPos) {
    auto chunk = getChunkShared(chunkPos);
    if (!chunk) return TopBlock();
    int localX = (worldPos.x % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;
    int localZ = (worldPos.y % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;
    return chunk->getTopBlock(localX, localZ);
}

TopBlock World::findBlockUnderPlayer(ivec2 chunkPos, ivec3 worldPos) {
    std::lock_guard<std::mutex> lock(_chunksMutex);

    auto it = _chunks.find({chunkPos.x, chunkPos.y});
    if (it == _chunks.end() || !it->second)
        return TopBlock();

    Chunk* chunk = it->second.get();

    const int localX = mod_floor(worldPos.x, CHUNK_SIZE);
    const int localZ = mod_floor(worldPos.z, CHUNK_SIZE);

    int startSubY   = floor_div(worldPos.y, CHUNK_SIZE);
    int startLocalY = mod_floor(worldPos.y, CHUNK_SIZE);

    return chunk->getFirstSolidBelow(localX, startLocalY, localZ, startSubY);
}

void World::init(int renderDistance) {
    _renderDistance = renderDistance;
    initGLBuffer();
}

void World::initBigSSBO(GLsizeiptr bytes) {
    if (_bigSSBO) glDeleteBuffers(1, &_bigSSBO);
    glCreateBuffers(1, &_bigSSBO);
    glNamedBufferStorage(_bigSSBO, bytes, nullptr, GL_DYNAMIC_STORAGE_BIT);
    _bigSSBOBytes = bytes;
    _useBigSSBO   = true;
}

World::~World() {
    {
        std::lock_guard<std::mutex> lock(_chunksMutex);
        _chunks.clear();
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

NoiseGenerator &World::getNoiseGenerator() { return _perlinGenerator; }
int *World::getRenderDistancePtr() { return &_renderDistance; }

BlockType World::getBlock(ivec2 chunkPos, ivec3 worldPos) {
    auto chunk = getChunkShared(chunkPos);
    if (!chunk) return AIR;
    int subChunkIndex = static_cast<int>(floor(worldPos.y / CHUNK_SIZE));
    SubChunk *subchunk = chunk->getSubChunk(subChunkIndex);
    if (!subchunk) return AIR;
    int localX = (worldPos.x % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;
    int localY = (worldPos.y % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;
    int localZ = (worldPos.z % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;
    return subchunk->getBlock({localX, localY, localZ});
}

bool World::setBlock(ivec2 chunkPos, ivec3 worldPos, BlockType value) {
    auto chunk = getChunkShared(chunkPos);
    if (!chunk) return false;

    const int subY = (int)std::floor((double)worldPos.y / (double)CHUNK_SIZE);
    SubChunk* sc = chunk->getOrCreateSubChunk(subY, /*generate=*/false);
    if (!sc) return false;

    const int lx = (worldPos.x % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;
    const int ly = (worldPos.y % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;
    const int lz = (worldPos.z % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;

    sc->setBlock(lx, ly, lz, value);

    markChunkDirty(chunkPos);
    return true;
}

bool World::setBlockOrQueue(ivec2 chunkPos, ivec3 worldPos, BlockType value) {
    auto chunk = getChunkShared(chunkPos);
    if (!chunk) {
        std::lock_guard<std::mutex> lk(_pendingMutex);
        _pendingEdits[chunkPos].push_back({worldPos, value});
        return false; // queued
    }
    return setBlock(chunkPos, worldPos, value); // applies immediately
}

void World::applyPendingFor(const ivec2& pos) {
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

    for (const auto& e : edits) setBlock(pos, e.worldPos, e.value);

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
    for (const auto& p : toRemesh) {
        if (auto c = getChunkShared(p)) c->sendFacesToDisplay();
    }
}

void World::setRunning(std::mutex *runningMutex, bool *isRunning) {
    _isRunning = isRunning;
    _runningMutex = runningMutex;
}

bool World::getIsRunning() {
    std::lock_guard<std::mutex> lockGuard(*_runningMutex);
    return *_isRunning;
}

void World::unloadChunk() {
    size_t count;
    { std::lock_guard<std::mutex> lk(_chunksListMutex); count = _chunkList.size(); }
    if (count <= CACHE_SIZE) return;

    ivec3 p = _camera.getWorldPosition();
    int pcx = p.x / CHUNK_SIZE; if (p.x < 0) --pcx;
    int pcz = p.z / CHUNK_SIZE; if (p.z < 0) --pcz;

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
        _chunkList.erase(farIt);
    }

    {
        std::lock_guard<std::mutex> lk(_displayedChunksMutex);
        if (_displayedChunks.count(pos)) {
            std::lock_guard<std::mutex> lk2(_chunksListMutex);
            _chunkList.emplace_back(victim);
            return;
        }
        _displayedChunks.erase(pos);
    }

    {
        std::lock_guard<std::mutex> lk(_chunksMutex);
        _chunks.erase(pos);
    }

    _memorySize -= victim->getMemorySize();

    if (victim.use_count() == 1) {
        _perlinGenerator.removePerlinMap(pos.x, pos.y);
    }
}

Chunk* World::loadChunk(int x, int z, int render, ivec2& chunkPos, int resolution) {
    auto sp = loadChunkShared(x, z, render, chunkPos, resolution);
    return sp.get();
}

std::shared_ptr<Chunk> World::loadChunkShared(int x, int z, int render, ivec2& chunkPos, int resolution) {
    ivec2 pos = { chunkPos.x - render / 2 + x, chunkPos.y - render / 2 + z };

    std::shared_ptr<Chunk> chunk;
    {
        std::lock_guard<std::mutex> lock(_chunksMutex);
        auto it = _chunks.find(pos);
        if (it != _chunks.end())
            chunk = it->second;
    }

    bool created = false;
    if (!chunk) {
        PerlinMap* pMap = _perlinGenerator.getPerlinMap(pos, resolution);
        auto newChunk = std::make_shared<Chunk>(pos, pMap, _caveGen, *this, _textureManager, _threadPool, resolution);

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

    if (chunk && chunk->getResolution() > resolution)
        chunk->updateResolution(resolution);

    if (chunk) {
        std::lock_guard<std::mutex> lock(_displayedChunksMutex);
        _displayedChunks[pos] = chunk;
    }

    return chunk;
}

void World::loadTopChunks(int render, ivec2 &chunkPos, int resolution) {
    int z = 0;
    for (int x = 0; x < render && getIsRunning(); x++)
        loadChunk(x, z, render, chunkPos, resolution);
}
void World::loadBotChunks(int render, ivec2 &chunkPos, int resolution) {
    int z = render - 1;
    for (int x = render - 1; getIsRunning() && x >= 0; x--)
        loadChunk(x, z, render, chunkPos, resolution);
}
void World::loadRightChunks(int render, ivec2 &chunkPos, int resolution) {
    int x = render - 1;
    for (int z = 1; z < render - 1 && getIsRunning(); z++) // avoid corners
        loadChunk(x, z, render, chunkPos, resolution);
}
void World::loadLeftChunks(int render, ivec2 &chunkPos, int resolution) {
    int x = 0;
    for (int z = render - 2; getIsRunning() && z > 0; z--) // avoid corners
        loadChunk(x, z, render, chunkPos, resolution);
}

void World::loadFirstChunks(ivec2 &chunkPos) {
    int renderDistance = _renderDistance;

    int resolution = RESOLUTION;
    _threshold = LOD_THRESHOLD;

    std::vector<std::future<void>> retLst;
    _currentRender = 0;
    for (int render = 0; getIsRunning() && render < renderDistance; render += 2)
    {
        std::future<void> retTop;
        std::future<void> retBot;
        std::future<void> retRight;
        std::future<void> retLeft;

        retTop   = _threadPool.enqueue(&World::loadTopChunks,   this, render, chunkPos, resolution);
        retBot   = _threadPool.enqueue(&World::loadRightChunks, this, render, chunkPos, resolution);
        retRight = _threadPool.enqueue(&World::loadBotChunks,   this, render, chunkPos, resolution);
        retLeft  = _threadPool.enqueue(&World::loadLeftChunks,  this, render, chunkPos, resolution);

        retTop.get(); retBot.get(); retRight.get(); retLeft.get();

        if (render >= _threshold && resolution < CHUNK_SIZE) {
            resolution *= 2;
            _threshold = _threshold * 2;
        }
        retLst.emplace_back(_threadPool.enqueue(&World::updateFillData, this));
        if (hasMoved(chunkPos)) break;
        _currentRender = render + 2;
    }
    for (auto &ret : retLst) ret.get();
}

int *World::getCurrentRenderPtr() { return &_currentRender; }
size_t *World::getMemorySizePtr() { return &_memorySize; }

void World::unLoadNextChunks(ivec2 &newCamChunk) {
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
        for (const auto &pos : toErase) _displayedChunks.erase(pos);
    }
    updateFillData();
}

void World::updateFillData() {
    // make sure queued edits + dirty meshes are up-to-date
    flushDirtyChunks();

    DisplayData* fillData        = new DisplayData();
    DisplayData* transparentData = new DisplayData();
    buildFacesToDisplay(fillData, transparentData);
    std::lock_guard<std::mutex> g(_drawDataMutex);
    _stagedDataQueue.emplace(fillData);
    _transparentStagedDataQueue.emplace(transparentData);
}

bool World::hasMoved(ivec2 &oldPos) {
    ivec2 camChunk = _camera.getChunkPosition(CHUNK_SIZE);
    if ((floor(oldPos.x) != floor(camChunk.x) || floor(oldPos.y) != floor(camChunk.y)))
        return true;
    return false;
}

SubChunk* World::getSubChunk(ivec3 &position) {
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
    return it->second;
}

void World::buildFacesToDisplay(DisplayData* fillData, DisplayData* transparentFillData) {
    // snapshot displayed chunks
    std::vector<std::shared_ptr<Chunk>> snapshot;
    {
        std::lock_guard<std::mutex> lk(_displayedChunksMutex);
        snapshot.reserve(_displayedChunks.size());
        for (auto &kv : _displayedChunks) snapshot.push_back(kv.second);
    }

    for (const auto& c : snapshot) {
        auto& ssbo = c->getSSBO();

        // SOLID
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

            size_t idx = std::min(i, ssboLimit ? ssboLimit - 1 : (size_t)0);
            glm::vec3 o = glm::vec3(ssbo[idx].x, ssbo[idx].y, ssbo[idx].z);
            fillData->cmdAABBsSolid.push_back({ o, o + glm::vec3(CHUNK_SIZE) });
        }

        fillData->ssboData.insert(fillData->ssboData.end(), ssbo.begin(), ssbo.end());
        fillData->chunkCmdRanges[c->getPosition()] = {
            startSolid, (uint32_t)(fillData->indirectBufferData.size() - startSolid)
        };

        // TRANSPARENT
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

void World::updateSSBO() {
    bool needUpdate = false;
    size_t size = _drawData->ssboData.size() * 2;
    while (size > _drawnSSBOSize) {
        _drawnSSBOSize *= 2;
        needUpdate = true;
    }
    if (needUpdate) {
        glDeleteBuffers(1, &_ssbo);
        glCreateBuffers(1, &_ssbo);
        glNamedBufferStorage(_ssbo, sizeof(vec4) * _drawnSSBOSize, nullptr, GL_DYNAMIC_STORAGE_BIT);
        glNamedBufferSubData(_ssbo, 0, sizeof(vec4) * _drawData->ssboData.size(), _drawData->ssboData.data());
    }
}

void World::updateDrawData() {
    std::lock_guard<std::mutex> lock(_drawDataMutex);
    if (!_stagedDataQueue.empty()) {
        DisplayData *stagedData = _stagedDataQueue.front();
        std::swap(stagedData, _drawData);
        delete stagedData;
        stagedData = nullptr;
        _stagedDataQueue.pop();
        _needUpdate = true;
    }
    if (!_transparentStagedDataQueue.empty()) {
        DisplayData *stagedData = _transparentStagedDataQueue.front();
        std::swap(stagedData, _transparentDrawData);
        delete stagedData;
        stagedData = nullptr;
        _transparentStagedDataQueue.pop();
        _needTransparentUpdate = true;
    }
    if (_drawData && (_needUpdate || _needTransparentUpdate))
        updateSSBO();

    if (_drawData) {
        if (_drawData->ssboData.size() != _drawData->indirectBufferData.size()) {
            std::cout << "[CULL] Mismatch solid: ssbo=" << _drawData->ssboData.size()
                      << " cmds=" << _drawData->indirectBufferData.size() << std::endl;
        }
    }
    if (_transparentDrawData && _drawData) {
        if (_drawData->ssboData.size() != _transparentDrawData->indirectBufferData.size()) {
            std::cout << "[CULL] Mismatch transp: ssbo=" << _drawData->ssboData.size()
                      << " cmds=" << _transparentDrawData->indirectBufferData.size() << std::endl;
        }
    }
}

int World::display() {
    if (!_drawData) return 0;
    if (_needUpdate) { pushVerticesToOpenGL(false); }

    const int outIx = _frameIx % kFrames;
    const int inIx  = _srcIxSolid;
    const GLsizei want = (GLsizei)_solidDrawCount;
    if (want == 0) return 0;

    runGpuCulling(false);

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, _bigSSBO);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, _pbSolidInstSSBO[inIx].id);

    glBindVertexArray(_vao);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, _pbSolidIndirect[outIx].id);
    glMultiDrawArraysIndirect(GL_TRIANGLE_STRIP, nullptr, want, 0);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
    glBindVertexArray(0);

    long long tris = 0;
    for (const auto& c : _drawData->indirectBufferData) {
        long long per = (c.count >= 3) ? (c.count - 2) : 0;
        tris += (long long)c.instanceCount * per;
    }
    return (int)tris;
}

int World::displayTransparent() {
    if (!_transparentDrawData) return 0;
    if (_needTransparentUpdate) { pushVerticesToOpenGL(true); }

    const int outIx = _frameIx % kFrames;
    const int inIx  = _srcIxTransp;
    const GLsizei want = (GLsizei)_transpDrawCount;
    if (want == 0) return 0;

    runGpuCulling(true);

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, _bigSSBO);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, _pbTranspInstSSBO[inIx].id);

    glDisable(GL_CULL_FACE);
    glBindVertexArray(_transparentVao);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, _pbTranspIndirect[outIx].id);
    glMultiDrawArraysIndirect(GL_TRIANGLE_STRIP, nullptr, want, 0);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
    glBindVertexArray(0);

    return (int)want;
}

void World::pushVerticesToOpenGL(bool transparent) {
    std::lock_guard<std::mutex> lock(_drawDataMutex);
    const int ix = _frameIx % kFrames;

    if (_inFlight[ix]) {
        GLenum s = glClientWaitSync(_inFlight[ix], 0, 0);
        if (s == GL_TIMEOUT_EXPIRED || s == GL_WAIT_FAILED) {
            glWaitSync(_inFlight[ix], 0, GL_TIMEOUT_IGNORED);
        }
        glDeleteSync(_inFlight[ix]); _inFlight[ix] = nullptr;
    }

    if (transparent)
    {
        const GLsizeiptr nCmd      = (GLsizeiptr)_transparentDrawData->indirectBufferData.size();
        const GLsizeiptr bytesCmd  = nCmd * sizeof(DrawArraysIndirectCommand);
        const GLsizeiptr nInst     = (GLsizeiptr)_transparentDrawData->vertexData.size();
        const GLsizeiptr bytesInst = nInst * sizeof(int);
        const GLsizeiptr bytesSSBO = (GLsizeiptr)_drawData->ssboData.size() * sizeof(glm::vec4);

        _pbTranspIndirect[ix].ensure(bytesCmd);
        _pbTranspTemplate[ix].ensure(bytesCmd);
        _pbTranspInstSSBO[ix].ensure(bytesInst);

        if (bytesCmd)  {
            memcpy(_pbTranspIndirect[ix].ptr, _transparentDrawData->indirectBufferData.data(), bytesCmd);
            memcpy(_pbTranspTemplate[ix].ptr, _transparentDrawData->indirectBufferData.data(), bytesCmd);
            _pbTranspIndirect[ix].flush(0, bytesCmd);
            _pbTranspTemplate[ix].flush(0, bytesCmd);
        }
        if (bytesInst) {
            memcpy(_pbTranspInstSSBO[ix].ptr, _transparentDrawData->vertexData.data(), bytesInst);
            _pbTranspInstSSBO[ix].flush(0, bytesInst);
        }

        if (_useBigSSBO && bytesSSBO) {
            if (bytesSSBO <= _bigSSBOBytes) {
                glNamedBufferSubData(_bigSSBO, 0, bytesSSBO, _drawData->ssboData.data());
            } else {
                initBigSSBO(bytesSSBO * 2);
                glNamedBufferSubData(_bigSSBO, 0, bytesSSBO, _drawData->ssboData.data());
            }
        }

        glMemoryBarrier(GL_CLIENT_MAPPED_BUFFER_BARRIER_BIT);

        _transpDrawCount = (GLsizei)nCmd;
        _needTransparentUpdate = false;
        _srcIxTransp = ix;
    }
    else
    {
        const GLsizeiptr nCmd      = (GLsizeiptr)_drawData->indirectBufferData.size();
        const GLsizeiptr bytesCmd  = nCmd * sizeof(DrawArraysIndirectCommand);
        const GLsizeiptr nInst     = (GLsizeiptr)_drawData->vertexData.size();
        const GLsizeiptr bytesInst = nInst * sizeof(int);
        const GLsizeiptr bytesSSBO = (GLsizeiptr)_drawData->ssboData.size() * sizeof(glm::vec4);

        _pbSolidIndirect[ix].ensure(bytesCmd);
        _pbSolidTemplate[ix].ensure(bytesCmd);
        _pbSolidInstSSBO[ix].ensure(bytesInst);

        if (bytesCmd)  {
            memcpy(_pbSolidIndirect[ix].ptr, _drawData->indirectBufferData.data(), bytesCmd);
            memcpy(_pbSolidTemplate[ix].ptr, _drawData->indirectBufferData.data(),  bytesCmd);
            _pbSolidIndirect[ix].flush(0, bytesCmd);
            _pbSolidTemplate[ix].flush(0, bytesCmd);
        }
        if (bytesInst) {
            memcpy(_pbSolidInstSSBO[ix].ptr, _drawData->vertexData.data(), bytesInst);
            _pbSolidInstSSBO[ix].flush(0, bytesInst);
        }

        if (_useBigSSBO && bytesSSBO) {
            if (bytesSSBO <= _bigSSBOBytes) {
                glNamedBufferSubData(_bigSSBO, 0, bytesSSBO, _drawData->ssboData.data());
            } else {
                initBigSSBO(bytesSSBO * 2);
                glNamedBufferSubData(_bigSSBO, 0, bytesSSBO, _drawData->ssboData.data());
            }
        }

        glMemoryBarrier(GL_CLIENT_MAPPED_BUFFER_BARRIER_BIT);

        _solidDrawCount = (GLsizei)nCmd;
        _needUpdate = false;
        _srcIxSolid = ix;
    }
}

void World::initGpuCulling() {
    _cullProgram  = compileComputeShader("shaders/compute/frustum_cull.glsl");
    _locNumDraws  = glGetUniformLocation(_cullProgram, "numDraws");
    _locChunkSize = glGetUniformLocation(_cullProgram, "chunkSize");

    glCreateBuffers(1, &_frustumUBO);
    glNamedBufferData(_frustumUBO, sizeof(glm::vec4) * 6, nullptr, GL_DYNAMIC_DRAW);
}

void World::initGLBuffer() {
    if (_hasBufferInitialized) return;

    glGenVertexArrays(1, &_vao);
    glGenBuffers(1, &_vbo);
    glGenBuffers(1, &_instanceVBO);
    glGenBuffers(1, &_indirectBuffer);
    glCreateBuffers(1, &_ssbo);

    GLint vertices[] = { 0,0,0, 1,0,0, 0,1,0, 1,1,0 };

    glBindVertexArray(_vao);
    glBindBuffer(GL_ARRAY_BUFFER, _vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glVertexAttribIPointer(0, 3, GL_INT, 3 * sizeof(GLint), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);

    glGenVertexArrays(1, &_transparentVao);
    glGenBuffers(1, &_transparentInstanceVBO);
    glGenBuffers(1, &_transparentIndirectBuffer);

    glBindVertexArray(_transparentVao);
    glBindBuffer(GL_ARRAY_BUFFER, _vbo);
    glVertexAttribIPointer(0, 3, GL_INT, 3 * sizeof(GLint), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);

    const GLsizeiptr initCmdCap  = 1 << 20;
    const GLsizeiptr initInstCap = 1 << 22;

    for (int i=0;i<kFrames;++i) {
        _pbSolidIndirect[i].create(initCmdCap,  true);
        _pbSolidTemplate[i].create(initCmdCap,  true);
        _pbTranspIndirect[i].create(initCmdCap, true);
        _pbTranspTemplate[i].create(initCmdCap, true);

        _pbSolidInstSSBO[i].create(initInstCap,  true);
        _pbTranspInstSSBO[i].create(initInstCap, true);
    }

    initBigSSBO(kBigSSBOBytes);
    initGpuCulling();
    _hasBufferInitialized = true;
}

void World::updateTemplateBuffer(bool transparent, GLsizeiptr byteSize) {
    if (transparent) {
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, _transpTemplIndirectBuffer);
        glBufferData(GL_SHADER_STORAGE_BUFFER, byteSize, _transparentDrawData->indirectBufferData.data(), GL_STATIC_DRAW);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
        _transpDrawCount = (GLsizei)_transparentDrawData->indirectBufferData.size();
    } else {
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, _templIndirectBuffer);
        glBufferData(GL_SHADER_STORAGE_BUFFER, byteSize, _drawData->indirectBufferData.data(), GL_STATIC_DRAW);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
        _solidDrawCount = (GLsizei)_drawData->indirectBufferData.size();
    }
}

void World::updatePerlinMapResolution(PerlinMap *pMap, int newResolution) {
    _perlinGenerator.updatePerlinMapResolution(pMap, newResolution);
}

void World::setViewProj(const glm::mat4& view, const glm::mat4& proj) {
    Frustum f = Frustum::fromVP(proj * view);
    glm::vec4 planes[6];
    for (int i = 0; i < 6; ++i)
        planes[i] = glm::vec4(f.p[i].n, f.p[i].d);
    glNamedBufferSubData(_frustumUBO, 0, sizeof(planes), planes);
}

void World::runGpuCulling(bool transparent) {
    const int outIx = _frameIx % kFrames;
    const int inIx  = transparent ? _srcIxTransp : _srcIxSolid;

    GLuint templ = transparent ? _pbTranspTemplate[inIx].id : _pbSolidTemplate[inIx].id;
    GLuint out   = transparent ? _pbTranspIndirect[outIx].id : _pbSolidIndirect[outIx].id;

    const GLsizei count = transparent ? _transpDrawCount : _solidDrawCount;
    if (count <= 0) return;

    glMemoryBarrier(GL_CLIENT_MAPPED_BUFFER_BARRIER_BIT);

    GLint prevProg = 0; glGetIntegerv(GL_CURRENT_PROGRAM, &prevProg);
    glUseProgram(_cullProgram);

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, _bigSSBO);
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

void World::endFrame() {
    const int ix = _frameIx % kFrames;
    if (_inFlight[ix]) { glDeleteSync(_inFlight[ix]); _inFlight[ix] = nullptr; }
    _inFlight[ix] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    _frameIx = (_frameIx + 1) % kFrames;
}
