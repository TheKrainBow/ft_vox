#include "Chunk.hpp"

Chunk::Chunk(ivec2 pos, PerlinMap *perlinMap, CaveGenerator &caveGen, World &world, TextureManager &textureManager, ThreadPool &pool, int resolution)
: _position(pos)
, _facesSent(false)
, _hasAllNeighbors(false)
, _isInit(false)
, _world(world)
, _textureManager(textureManager)
, _perlinMap(perlinMap)
, _hasBufferInitialized(false)
, _needUpdate(true)
, _caveGen(caveGen)
, _resolution(resolution)
, _pool(pool)
{
}

void Chunk::loadBlocks() {
    int heighest = _perlinMap->heighest;
    if (heighest < OCEAN_HEIGHT) heighest = OCEAN_HEIGHT;

    const int minYIdx = 0;
    const int maxYIdx = (heighest / CHUNK_SIZE) + 1;

    std::vector<std::future<std::pair<int, SubChunk*>>> futures;
    futures.reserve(maxYIdx - minYIdx + 1);

    for (int idx = minYIdx; idx <= maxYIdx; ++idx) {
        futures.emplace_back(_pool.enqueue([this, idx]() -> std::pair<int, SubChunk*>
        {
            auto* sub = new SubChunk(
                { _position.x, idx, _position.y },
                _perlinMap, _caveGen, *this, _world, _textureManager, _resolution
            );
            sub->loadHeight(0);
            sub->loadBiome(0);
            return { idx, sub };
        }));
    }

    size_t localMem = 0;
    for (auto& f : futures) {
        auto [idx, generated] = f.get();

        SubChunk* existing = nullptr;
        {
            std::lock_guard<std::mutex> lk(_subChunksMutex);
            auto it = _subChunks.find(idx);
            if (it == _subChunks.end()) {
                _subChunks.emplace(idx, generated);
                existing = nullptr;
            } else {
                existing = it->second;
                it->second = generated;
            }
        }

        if (existing) {
            for (int y = 0; y < CHUNK_SIZE; y += _resolution) {
                for (int z = 0; z < CHUNK_SIZE; z += _resolution) {
                    for (int x = 0; x < CHUNK_SIZE; x += _resolution) {
                        char b = existing->getBlock({x, y, z});
                        if (b != AIR) {
                            generated->setBlock(x, y, z, b);
                        }
                    }
                }
            }
            delete existing;
        }
        localMem += generated->getMemorySize();
    }

    _memorySize += localMem;
    _isInit = true;
    _memorySize += sizeof(*this);
}

size_t Chunk::getMemorySize() { return _memorySize; }

TopBlock Chunk::getTopBlock(int localX, int localZ) {
    std::lock_guard<std::mutex> lock(_subChunksMutex);
    int maxIdx = INT_MIN;
    for (const auto& kv : _subChunks) maxIdx = std::max(maxIdx, kv.first);
    if (maxIdx == INT_MIN) return {0, 0, {0.0, 0.0}};

    for (int subY = maxIdx; subY >= 0; --subY) {
        auto it = _subChunks.find(subY);
        if (it == _subChunks.end() || !it->second) continue;

        SubChunk* sub = it->second;
        for (int y = CHUNK_SIZE - 1; y >= 0; --y) {
            uint8_t block = sub->getBlock({localX, y, localZ});
            if (block != AIR && block != WATER)
                return {subY * CHUNK_SIZE + y, (char)block, {0.0, 0.0}};
        }
    }
    return {0, 0, {0.0, 0.0}};
}

TopBlock Chunk::getFirstSolidBelow(int localX, int startLocalY, int localZ, int startSubY) {
    std::lock_guard<std::mutex> lock(_subChunksMutex);

    int highest = -1;
    for (const auto &kv : _subChunks)
        if (kv.first > highest) highest = kv.first;

    if (highest < 0)               return {0, 0, {0.0, 0.0}};
    if (startSubY > highest) {
        startSubY   = highest;
        startLocalY = CHUNK_SIZE - 1;
    }
    if (startSubY < 0)             return {0, 0, {0.0, 0.0}};

    // strictly search below the starting voxel
    if (startLocalY > 0) {
        --startLocalY;
    } else {
        --startSubY;
        startLocalY = CHUNK_SIZE - 1;
    }

    for (int subY = startSubY; subY >= 0; --subY) {
        auto it = _subChunks.find(subY);
        if (it == _subChunks.end() || !it->second) continue;

        SubChunk* sub = it->second;
        const int yStart = (subY == startSubY) ? startLocalY : (CHUNK_SIZE - 1);

        for (int y = yStart; y >= 0; --y) {
            const uint8_t block = sub->getBlock({localX, y, localZ});
            if (block != AIR && block != WATER) {
                return { subY * CHUNK_SIZE + y, static_cast<char>(block), {0.0, 0.0} };
            }
        }
    }
    return {0, 0, {0.0, 0.0}};
}

Chunk::~Chunk() {
    for (auto &subchunk : _subChunks) delete subchunk.second;
    _subChunks.clear();
}

void Chunk::getNeighbors() {
    ivec2 northPos{_position.x, _position.y - 1};
    ivec2 southPos{_position.x, _position.y + 1};
    ivec2 eastPos {_position.x + 1, _position.y};
    ivec2 westPos {_position.x - 1, _position.y};

    auto n = _world.getChunkShared(northPos);
    auto s = _world.getChunkShared(southPos);
    auto e = _world.getChunkShared(eastPos);
    auto w = _world.getChunkShared(westPos);

    _north = n; _south = s; _east = e; _west = w;
    updateHasAllNeighbors();

    if (n) { n->setSouthChunk(shared_from_this()); n->sendFacesToDisplay(); }
    if (s) { s->setNorthChunk(shared_from_this()); s->sendFacesToDisplay(); }
    if (e) { e->setWestChunk (shared_from_this()); e->sendFacesToDisplay(); }
    if (w) { w->setEastChunk (shared_from_this()); w->sendFacesToDisplay(); }
}

void Chunk::updateHasAllNeighbors() {
    _hasAllNeighbors =
        !_north.expired() && !_south.expired() &&
        !_east .expired() && !_west .expired();
}

void Chunk::unloadNeighbors() {
    if (auto n = _north.lock()) n->unloadNeighbor(SOUTH);
    if (auto s = _south.lock()) s->unloadNeighbor(NORTH);
    if (auto e = _east .lock()) e->unloadNeighbor(WEST);
    if (auto w = _west .lock()) w->unloadNeighbor(EAST);
}

void Chunk::unloadNeighbor(Direction dir) {
    switch (dir) {
        case NORTH: _north.reset(); break;
        case SOUTH: _south.reset(); break;
        case EAST:  _east .reset(); break;
        case WEST:  _west .reset(); break;
        case DOWN:  break;
        case UP:    break;
    }
    updateHasAllNeighbors();
}

ivec2 Chunk::getPosition() { return _position; }

SubChunk* Chunk::getSubChunk(int y) {
    if (_isInit == false) return nullptr;
    std::lock_guard<std::mutex> lk(_subChunksMutex);
    auto it = _subChunks.find(y);
    return (it != _subChunks.end()) ? it->second : nullptr;
}

SubChunk* Chunk::getOrCreateSubChunk(int subY, bool generate) {
    {
        std::lock_guard<std::mutex> lk(_subChunksMutex);
        auto it = _subChunks.find(subY);
        if (it != _subChunks.end()) return it->second;
    }

    auto* sc = new SubChunk(
        { _position.x, subY, _position.y },
        _perlinMap, _caveGen, *this, _world, _textureManager, _resolution
    );

    if (generate) {
        sc->loadHeight(_resolution);
        sc->loadBiome (_resolution);
    } else {
        sc->markLoaded(true);
    }

    {
        std::lock_guard<std::mutex> lk(_subChunksMutex);
        auto [it, inserted] = _subChunks.emplace(subY, sc);
        if (!inserted) { delete sc; return it->second; }
        return sc;
    }
}

bool Chunk::isReady() { return _facesSent; }

void Chunk::clearFaces() {
    _vertexData.clear();
    _indirectBufferData.clear();
    _transparentVertexData.clear();
    _transparentIndirectBufferData.clear();
    _ssboData.clear();
}

void Chunk::sendFacesToDisplay() {
    if (_isInit == false) return;

    std::vector<SubChunk*> subs;
    {
        std::lock_guard<std::mutex> lk(_subChunksMutex);
        subs.reserve(_subChunks.size());
        for (auto& kv : _subChunks) subs.push_back(kv.second);
    }

    std::lock_guard<std::mutex> guard(_sendFacesMutex);
    clearFaces();

    for (auto* sc : subs)
    {
        sc->sendFacesToDisplay();
        auto& vertices            = sc->getVertices();
        auto& transparentVertices = sc->getTransparentVertices();

        _indirectBufferData.push_back(DrawArraysIndirectCommand{
            4, uint(vertices.size()), 0, uint(_vertexData.size()),
        });

        _transparentIndirectBufferData.push_back(DrawArraysIndirectCommand{
            4, uint(transparentVertices.size()), 0, uint(_transparentVertexData.size()),
        });

        ivec3 pos = sc->getPosition();
        _ssboData.push_back(vec4{
            pos.x * CHUNK_SIZE, pos.y * CHUNK_SIZE, pos.z * CHUNK_SIZE, _resolution.load()
        });

        _vertexData.insert(_vertexData.end(), vertices.begin(), vertices.end());
        _transparentVertexData.insert(_transparentVertexData.end(),
                                      transparentVertices.begin(), transparentVertices.end());
    }
    _facesSent = true;
}

void Chunk::setNorthChunk(const std::shared_ptr<Chunk>& c) { _north = c; updateHasAllNeighbors(); }
void Chunk::setSouthChunk(const std::shared_ptr<Chunk>& c) { _south = c; updateHasAllNeighbors(); }
void Chunk::setEastChunk (const std::shared_ptr<Chunk>& c) { _east  = c; updateHasAllNeighbors(); }
void Chunk::setWestChunk (const std::shared_ptr<Chunk>& c) { _west  = c; updateHasAllNeighbors(); }

std::shared_ptr<Chunk> Chunk::getNorthChunk() const { return _north.lock(); }
std::shared_ptr<Chunk> Chunk::getSouthChunk() const { return _south.lock(); }
std::shared_ptr<Chunk> Chunk::getEastChunk () const { return _east .lock(); }
std::shared_ptr<Chunk> Chunk::getWestChunk () const { return _west .lock(); }

std::vector<int> &Chunk::getVertices() {
    std::lock_guard<std::mutex> lock(_sendFacesMutex);
    return _vertexData;
}
std::vector<DrawArraysIndirectCommand> &Chunk::getIndirectData() {
    std::lock_guard<std::mutex> lock(_sendFacesMutex);
    return _indirectBufferData;
}
std::vector<vec4> &Chunk::getSSBO() {
    std::lock_guard<std::mutex> lock(_sendFacesMutex);
    return _ssboData;
}
std::vector<int> &Chunk::getTransparentVertices() {
    std::lock_guard<std::mutex> lock(_sendFacesMutex);
    return _transparentVertexData;
}
std::vector<DrawArraysIndirectCommand> &Chunk::getTransparentIndirectData() {
    std::lock_guard<std::mutex> lock(_sendFacesMutex);
    return _transparentIndirectBufferData;
}

void Chunk::freeSubChunks() {
    _subChunksMutex.lock();
    for (auto &subchunk : _subChunks) {
        if (subchunk.second) delete subchunk.second;
        subchunk.second = nullptr;
    }
    _subChunksMutex.unlock();
    _subChunks.clear();
}

void Chunk::updateResolution(int newResolution) {
    _resolution = newResolution;

    std::vector<SubChunk*> subs;
    {
        std::lock_guard<std::mutex> lk(_subChunksMutex);
        subs.reserve(_subChunks.size());
        for (auto& kv : _subChunks) subs.push_back(kv.second);
    }
    for (auto* sc : subs) sc->updateResolution(newResolution, _perlinMap);

    _facesSent = false;
    sendFacesToDisplay();

    if (auto n = _north.lock()) n->sendFacesToDisplay();
    if (auto s = _south.lock()) s->sendFacesToDisplay();
    if (auto e = _east .lock()) e->sendFacesToDisplay();
    if (auto w = _west .lock()) w->sendFacesToDisplay();
}

void Chunk::getAABB(glm::vec3& minp, glm::vec3& maxp) {
    const float xs = _position.x * CHUNK_SIZE;
    const float zs = _position.y * CHUNK_SIZE;

    int minIdx = INT_MAX, maxIdx = INT_MIN;
    {
        std::lock_guard<std::mutex> lock(_subChunksMutex);
        for (const auto& kv : _subChunks) {
            minIdx = std::min(minIdx, kv.first);
            maxIdx = std::max(maxIdx, kv.first);
        }
    }
    if (minIdx == INT_MAX) {
        minp = { xs, 0.0f,  zs };
        maxp = { xs + CHUNK_SIZE, CHUNK_SIZE, zs + CHUNK_SIZE };
        return;
    }
    minp = { xs,               float(minIdx * CHUNK_SIZE), zs };
    maxp = { xs + CHUNK_SIZE,  float((maxIdx + 1) * CHUNK_SIZE), zs + CHUNK_SIZE };
}

std::atomic_int &Chunk::getResolution() { return _resolution; }

void Chunk::snapshotDisplayData(
    std::vector<int>& outSolidVerts,
    std::vector<DrawArraysIndirectCommand>& outSolidCmds,
    std::vector<vec4>& outSSBO,
    std::vector<int>& outTranspVerts,
    std::vector<DrawArraysIndirectCommand>& outTranspCmds)
{
    std::lock_guard<std::mutex> lockk(_sendFacesMutex);
    outSolidVerts   = _vertexData;
    outSolidCmds    = _indirectBufferData;
    outSSBO         = _ssboData;
    outTranspVerts  = _transparentVertexData;
    outTranspCmds   = _transparentIndirectBufferData;
}
