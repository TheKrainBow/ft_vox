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
	_ssboData.clear();
}

void Chunk::sendFacesToDisplay()
{
	if (_hasAllNeighbors == false || _isInit == false)
		return ;

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
	for (SubChunk* sc : subs) {
		if (!sc) continue;
		sc->sendFacesToDisplay();
		totalSolid += sc->getVertices().size();
		totalTrans += sc->getTransparentVertices().size();
	}

	// Reserve to reduce reallocations
	_indirectBufferData.reserve(_indirectBufferData.size() + subs.size());
	_transparentIndirectBufferData.reserve(_transparentIndirectBufferData.size() + subs.size());
	_ssboData.reserve(_ssboData.size() + subs.size());
	_vertexData.reserve(_vertexData.size() + totalSolid);
	_transparentVertexData.reserve(_transparentVertexData.size() + totalTrans);

	// Second pass: emit draw commands and append streams (no extra copies)
	for (SubChunk* sc : subs)
	{
		if (!sc) continue;

		auto &vertices            = sc->getVertices();
		auto &transparentVertices = sc->getTransparentVertices();

		// ---- SOLID draw cmd ----
		_indirectBufferData.push_back(DrawArraysIndirectCommand{
			4,
			static_cast<uint32_t>(vertices.size()),
			0,
			static_cast<uint32_t>(_vertexData.size())
		});

		// ---- TRANSPARENT draw cmd ----
		_transparentIndirectBufferData.push_back(DrawArraysIndirectCommand{
			4,
			static_cast<uint32_t>(transparentVertices.size()),
			0,
			static_cast<uint32_t>(_transparentVertexData.size())
		});

		// Per-draw origin/res for both passes
		ivec3 pos = sc->getPosition();
		_ssboData.push_back(vec4{
			pos.x * CHUNK_SIZE, pos.y * CHUNK_SIZE, pos.z * CHUNK_SIZE, _resolution.load()
		});

		// Append instance streams
		_vertexData.insert(_vertexData.end(), vertices.begin(), vertices.end());
		_transparentVertexData.insert(_transparentVertexData.end(), transparentVertices.begin(), transparentVertices.end());
	}
	_facesSent = true;
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

bool Chunk::isBuilding() const { return _isBuilding.load(); }

// If a block was destroyed/placed mark this chunk dirty
void Chunk::setAsModified() { _isModified = true; };
bool Chunk::getModified() const { return _isModified.load(); }
