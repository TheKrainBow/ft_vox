#include "Chunk.hpp"

Chunk::Chunk(ivec2 pos, PerlinMap *perlinMap, CaveGenerator &caveGen, ChunkLoader &chunkLoader, ThreadPool &pool, int resolution)
:
_position(pos),
_facesSent(false),
_hasAllNeighbors(false),
_isInit(false),
_chunkLoader(chunkLoader),
_perlinMap(perlinMap),
_caveGen(caveGen),
_resolution(resolution),
_pool(pool),
_isBuilding(false),
_isModified(false)
{
    _north = _south = _east = _west = nullptr;
}

void Chunk::loadBlocks() {
	// Disallow external edits when building
	_isBuilding = true;

	int heighest = _perlinMap->heighest;
	if (heighest < OCEAN_HEIGHT) heighest = OCEAN_HEIGHT;

	const int minYIdx = 0;
	const int maxYIdx = (heighest / CHUNK_SIZE) + 1;

	std::vector<std::future<std::pair<int, SubChunk*>>> futures;
	futures.reserve(maxYIdx - minYIdx + 1);

	for (int idx = minYIdx; _chunkLoader.getIsRunning() && idx <= maxYIdx; ++idx) {
		futures.emplace_back(_pool.enqueue([this, idx]() -> std::pair<int, SubChunk*>
		{
			int res = _resolution.load();
			auto* sub = new SubChunk(
				{ _position.x, idx, _position.y },
				_perlinMap, _caveGen, *this, _chunkLoader, res
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
			} else {
				existing = it->second;
				it->second = generated;
			}
		}
		if (existing) {
			// migrate non-air, then destroy
			for (int y = 0; y < CHUNK_SIZE; y += _resolution)
			for (int z = 0; z < CHUNK_SIZE; z += _resolution)
			for (int x = 0; x < CHUNK_SIZE; x += _resolution) {
				char b = existing->getBlock({x, y, z});
				if (b != AIR) generated->setBlock(x, y, z, b);
			}
			delete existing;
		}
		localMem += generated->getMemorySize();
	}

	_memorySize += localMem;
	_isInit = true;
	_memorySize += sizeof(*this);
	// Allow external edits when not building
	_isBuilding = false;
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
        // Treat decorative plants as non-solid for ground queries
        if (block != AIR && block != WATER &&
            block != FLOWER_POPPY && block != FLOWER_DANDELION &&
            block != FLOWER_CYAN && block != FLOWER_SHORT_GRASS && block != FLOWER_DEAD_BUSH)
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
            // Ignore decorative plants for collision/ground detection
            if (block != AIR && block != WATER &&
                block != FLOWER_POPPY && block != FLOWER_DANDELION &&
                block != FLOWER_CYAN && block != FLOWER_SHORT_GRASS && block != FLOWER_DEAD_BUSH) {
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

void Chunk::getNeighbors()
{
	ivec2 northPos{_position.x, _position.y - 1};
	ivec2 southPos{_position.x, _position.y + 1};
	ivec2 eastPos{_position.x + 1, _position.y};
	ivec2 westPos{_position.x - 1, _position.y};
	_north = _chunkLoader.getChunk(northPos);
	_south = _chunkLoader.getChunk(southPos);
	_east = _chunkLoader.getChunk(eastPos);
	_west = _chunkLoader.getChunk(westPos);

	// Update our own neighbor-completeness flag so we can emit faces even if
	// all neighbors already existed before this chunk was created.
	_hasAllNeighbors = _north && _south && _east && _west;

	if (_north) {
		_north->setSouthChunk(this);
		_north->sendFacesToDisplay();
	}
	if (_south) {
		_south->setNorthChunk(this);
		_south->sendFacesToDisplay();
	}
	if (_east) {
		_east->setWestChunk(this);
		_east->sendFacesToDisplay();
	}
	if (_west) {
		_west->setEastChunk(this);
		_west->sendFacesToDisplay();
	}

	// If we are now fully surrounded, generate our faces immediately.
	if (_hasAllNeighbors)
		sendFacesToDisplay();
}

void Chunk::updateHasAllNeighbors() {
	_hasAllNeighbors =
		_north && _south &&
		_east  && _west;
}

void Chunk::unloadNeighbors() {
	if (_north) _north->unloadNeighbor(SOUTH);
	if (_south) _south->unloadNeighbor(NORTH);
	if (_east) _east->unloadNeighbor(WEST);
	if (_west) _west->unloadNeighbor(EAST);
}

void Chunk::unloadNeighbor(Direction dir) {
	switch (dir) {
		case NORTH: _north = nullptr; break;
		case SOUTH: _south = nullptr; break;
		case EAST:  _east = nullptr; break;
		case WEST:  _west = nullptr; break;
		case DOWN:	break;
		case UP:	break;
	}
	updateHasAllNeighbors();
}

ivec2 Chunk::getPosition() { return _position; }

SubChunk *Chunk::getSubChunk(int y)
{
	if (_isInit == false)
		return nullptr;
	std::lock_guard<std::mutex> lk(_subChunksMutex);
	auto it = _subChunks.find(y);
	if (it != _subChunks.end())
		return it->second;
	return nullptr;
}

SubChunk* Chunk::getOrCreateSubChunk(int subY, bool generate) {
	{
		std::lock_guard<std::mutex> lk(_subChunksMutex);
		auto it = _subChunks.find(subY);
		if (it != _subChunks.end()) return it->second;
	}

	int res = _resolution.load();
	auto* sc = new SubChunk(
		{ _position.x, subY, _position.y },
		_perlinMap, _caveGen, *this, _chunkLoader, res
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
    _ssboSolid.clear();
    _ssboTransp.clear();
    _metaSolid.clear();
    _metaTransp.clear();
}

void Chunk::sendFacesToDisplay()
{
	// Build faces even if not fully surrounded yet. Missing neighbors are treated
	// as transparent at borders; when neighbors arrive, both chunks will be
	// remeshed to resolve seams.
	if (_isInit == false)
		return;

	std::vector<SubChunk*> subs;
	{
		std::lock_guard<std::mutex> lk(_subChunksMutex);
		subs.reserve(_subChunks.size());
		for (auto &kv : _subChunks)
			subs.push_back(kv.second);
	}

	std::lock_guard<std::mutex> lkSend(_sendFacesMutex);
	clearFaces();

    // First pass: build subchunk meshes and compute total sizes to reserve
    size_t totalSolid = 0;
    size_t totalTrans = 0;
    size_t badDirSum  = 0;
    for (SubChunk* sc : subs) {
        if (!sc) continue;
        sc->sendFacesToDisplay();
        totalSolid += sc->getVertices().size();
        totalTrans += sc->getTransparentVertices().size();
        // Debug: verify per-direction counts sum to total
        const int* dc  = sc->getDirCounts();
        const int* tdc = sc->getTranspDirCounts();
        size_t sumS = 0, sumT = 0;
        if (dc)  for (int i=0;i<6;++i) sumS += (size_t)dc[i];
        if (tdc) for (int i=0;i<6;++i) sumT += (size_t)tdc[i];
        if (sumS != sc->getVertices().size() || sumT != sc->getTransparentVertices().size()) {
            ++badDirSum;
        }
    }

	// Reserve to reduce reallocations
    // Up to 6 draws per subchunk (one per face direction)
    _indirectBufferData.reserve(_indirectBufferData.size() + subs.size() * 6);
    _transparentIndirectBufferData.reserve(_transparentIndirectBufferData.size() + subs.size() * 6);
    _ssboSolid.reserve(_ssboSolid.size() + subs.size() * 6);
    _ssboTransp.reserve(_ssboTransp.size() + subs.size() * 6);
    _metaSolid.reserve(_metaSolid.size() + subs.size() * 6);
    _metaTransp.reserve(_metaTransp.size() + subs.size() * 6);
    // Reserve draw meta arrays in staged data (filled by ChunkLoader)
	_vertexData.reserve(_vertexData.size() + totalSolid);
	_transparentVertexData.reserve(_transparentVertexData.size() + totalTrans);

	// Second pass: emit draw commands and append streams (no extra copies)
    for (SubChunk* sc : subs)
    {
        if (!sc) continue;

        auto &vertices            = sc->getVertices();
        auto &transparentVertices = sc->getTransparentVertices();
        const int* dirCounts      = sc->getDirCounts();
        const int* tDirCounts     = sc->getTranspDirCounts();

        // Base offsets before appending this subchunk's vertices
        uint32_t baseSolidOffset = (uint32_t)_vertexData.size();
        uint32_t baseTransOffset = (uint32_t)_transparentVertexData.size();

        // Append instance streams (kept in directional order by SubChunk)
        _vertexData.insert(_vertexData.end(), vertices.begin(), vertices.end());
        _transparentVertexData.insert(_transparentVertexData.end(), transparentVertices.begin(), transparentVertices.end());

        // Per-direction draws for SOLID
        ivec3 pos = sc->getPosition();
        uint32_t running = baseSolidOffset;
        int order[6] = { UP, DOWN, NORTH, SOUTH, EAST, WEST };
        for (int ii = 0; ii < 6; ++ii) {
            int d = order[ii];
            int count = dirCounts ? dirCounts[d] : 0;
            if (count <= 0) continue;
            _indirectBufferData.push_back(DrawArraysIndirectCommand{ 4, (uint32_t)count, 0, running });
            _ssboSolid.push_back(vec4{ pos.x * CHUNK_SIZE, pos.y * CHUNK_SIZE, pos.z * CHUNK_SIZE, _resolution.load() });
            _metaSolid.push_back((uint32_t)d);
            running += (uint32_t)count;
        }
        // Per-direction draws for TRANSPARENT
        running = baseTransOffset;
        int torder[6] = { UP, DOWN, NORTH, SOUTH, EAST, WEST };
        for (int ii = 0; ii < 6; ++ii) {
            int d = torder[ii];
            int count = tDirCounts ? tDirCounts[d] : 0;
            if (count <= 0) continue;
            _transparentIndirectBufferData.push_back(DrawArraysIndirectCommand{ 4, (uint32_t)count, 0, running });
            // duplicate ssbo origin for matching draw (transparent pass uses same origins)
            _ssboTransp.push_back(vec4{ pos.x * CHUNK_SIZE, pos.y * CHUNK_SIZE, pos.z * CHUNK_SIZE, _resolution.load() });
            _metaTransp.push_back((uint32_t)d);
            running += (uint32_t)count;
        }

        // Discover and record any flower cells in this subchunk for the renderer
        if (_resolution == 1)
            _chunkLoader.scanAndRecordFlowersFor(_position, pos.y, sc, _resolution.load());
    }
    _facesSent = true;
    (void)badDirSum;
}

void Chunk::setNorthChunk(Chunk *c) { _north = c; updateHasAllNeighbors(); }
void Chunk::setSouthChunk(Chunk *c) { _south = c; updateHasAllNeighbors(); }
void Chunk::setEastChunk (Chunk *c) { _east  = c; updateHasAllNeighbors(); }
void Chunk::setWestChunk (Chunk *c) { _west  = c; updateHasAllNeighbors(); }
Chunk *Chunk::getNorthChunk() { return _north; }
Chunk *Chunk::getSouthChunk() { return _south; }
Chunk *Chunk::getEastChunk () { return _east;  }
Chunk *Chunk::getWestChunk () { return _west;  }

std::vector<int> &Chunk::getVertices() {
	std::lock_guard<std::mutex> lock(_sendFacesMutex);
	return _vertexData;
}
std::vector<DrawArraysIndirectCommand> &Chunk::getIndirectData() {
	std::lock_guard<std::mutex> lock(_sendFacesMutex);
	return _indirectBufferData;
}
std::vector<vec4> &Chunk::getSSBOSolid() { std::lock_guard<std::mutex> lock(_sendFacesMutex); return _ssboSolid; }
std::vector<vec4> &Chunk::getSSBOTransp() { std::lock_guard<std::mutex> lock(_sendFacesMutex); return _ssboTransp; }
std::vector<int> &Chunk::getTransparentVertices() {
	std::lock_guard<std::mutex> lock(_sendFacesMutex);
	return _transparentVertexData;
}
std::vector<DrawArraysIndirectCommand> &Chunk::getTransparentIndirectData() {
    std::lock_guard<std::mutex> lock(_sendFacesMutex);
    return _transparentIndirectBufferData;
}

bool Chunk::hasAnyDraws() {
    std::lock_guard<std::mutex> lock(_sendFacesMutex);
    return (!_indirectBufferData.empty() || !_transparentIndirectBufferData.empty());
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
	// No work if the resolution is already up-to-date.
	const int current = _resolution.load();
	if (newResolution == current)
		return;

	// Refresh the perlin data at the requested LOD before rebuilding subchunks.
	PerlinMap *updatedMap = _chunkLoader.getNoiseGenerator().getPerlinMap(_position, newResolution);
	if (updatedMap)	
		_perlinMap = updatedMap;

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

	if (_north) _north->sendFacesToDisplay();
	if (_south) _south->sendFacesToDisplay();
	if (_east ) _east->sendFacesToDisplay();
	if (_west ) _west->sendFacesToDisplay();
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
    std::vector<vec4>& outSSBOSolid,
    std::vector<int>& outTranspVerts,
    std::vector<DrawArraysIndirectCommand>& outTranspCmds,
    std::vector<vec4>& outSSBOTransp,
    std::vector<uint32_t>& outMetaSolid,
    std::vector<uint32_t>& outMetaTransp)
{
    std::lock_guard<std::mutex> lockk(_sendFacesMutex);
    outSolidVerts   = _vertexData;
    outSolidCmds    = _indirectBufferData;
    outSSBOSolid    = _ssboSolid;
    outTranspVerts  = _transparentVertexData;
    outTranspCmds   = _transparentIndirectBufferData;
    outSSBOTransp   = _ssboTransp;
    outMetaSolid    = _metaSolid;
    outMetaTransp   = _metaTransp;
}

bool Chunk::isBuilding() const { return _isBuilding.load(); }

// If a block was destroyed/placed mark this chunk dirty
void Chunk::setAsModified() { _isModified = true; };
bool Chunk::getModified() const { return _isModified.load(); }
