#include "Chunk.hpp"


Chunk::Chunk(ivec2 pos, PerlinMap *perlinMap, CaveGenerator &caveGen, World &world, TextureManager &textureManager, ThreadPool &pool, int resolution)
:
_position(pos),
_facesSent(false),
_hasAllNeighbors(false),
_isInit(false),
_world(world),
_textureManager(textureManager),
_perlinMap(perlinMap),
_hasBufferInitialized(false),
_needUpdate(true),
_caveGen(caveGen),
_resolution(resolution),
_pool(pool)
{
}

void Chunk::loadBlocks()
{
	// Compute the vertical range we actually need:
	// - Start at 0 (bedrock is at globalY==0 inside subchunk 0)
	// - Go up to the max of (chunk highest) and (ocean) + one extra subchunk
	int heighest = _perlinMap->heighest;
	if (heighest < OCEAN_HEIGHT) heighest = OCEAN_HEIGHT;

	const int minYIdx = 0;
	const int maxYIdx = (heighest / CHUNK_SIZE) + 1; // one subchunk margin above

	// Generate each subchunk in parallel, then insert them
	std::vector<std::future<std::pair<int, SubChunk*>>> futures;
	futures.reserve(maxYIdx - minYIdx + 1);

	for (int idx = minYIdx; idx <= maxYIdx; ++idx)
	{
		futures.emplace_back(
			_pool.enqueue([this, idx]() -> std::pair<int, SubChunk*>
			{
				SubChunk *sub = new SubChunk(
					{ _position.x, idx, _position.y },
					_perlinMap,
					_caveGen,
					*this,
					_world,
					_textureManager,
					_resolution
				);
				// Height then biome (biome only touches a few cells near the surface)
				sub->loadHeight(0);
				sub->loadBiome(0);
				return { idx, sub };
			})
		);
	}

	size_t localMem = 0;
	for (auto &f : futures)
	{
		auto [idx, sub] = f.get();
		_subChunks[idx] = sub;
		localMem += sub->getMemorySize();
	}
	_memorySize += localMem;

	_isInit = true;
	_memorySize += sizeof(*this);
}

size_t Chunk::getMemorySize() {
	return _memorySize;
}

TopBlock Chunk::getTopBlock(int localX, int localZ)
{
	std::lock_guard<std::mutex> lock(_subChunksMutex);
	int index = std::numeric_limits<int>::min();
	for (auto &elem : _subChunks)
	{
		if (elem.first > index)
			index = elem.first;
	}

	for (int subY = index; subY >= 0; subY--)
	{
		SubChunk *subchunk = _subChunks[subY];
		if (!subchunk)
			continue ;

		for (int y = CHUNK_SIZE - 1; y >= 0; --y)
		{
			uint8_t block = subchunk->getBlock({localX, y, localZ});
			if (block != AIR && block != WATER)
			{
				return {subY * CHUNK_SIZE + y, (char)block, {0.0, 0.0}};
			}
		}
	}
	return {0, 0, {0.0, 0.0}};
}

TopBlock Chunk::getFirstSolidBelow(int localX, int startLocalY, int localZ, int startSubY)
{
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

	const bool strictlyBelow = true;
	if (strictlyBelow) {
		if (startLocalY > 0) {
			--startLocalY;
		} else {
			--startSubY;
			startLocalY = CHUNK_SIZE - 1;
		}
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

Chunk::~Chunk()
{
	for (auto &subchunk : _subChunks)
		delete subchunk.second;
	_subChunks.clear();
}

void Chunk::getNeighbors()
{
	ivec2 northPos{_position.x, _position.y - 1};
	ivec2 southPos{_position.x, _position.y + 1};
	ivec2 eastPos{_position.x + 1, _position.y};
	ivec2 westPos{_position.x - 1, _position.y};
	_north = _world.getChunk(northPos);
	_south = _world.getChunk(southPos);
	_east = _world.getChunk(eastPos);
	_west = _world.getChunk(westPos);

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
}

void Chunk::unloadNeighbors()
{
	if (_north)
		_north->unloadNeighbor(SOUTH);
	if (_south)
		_south->unloadNeighbor(NORTH);
	if (_east)
		_east->unloadNeighbor(WEST);
	if (_west)
		_west->unloadNeighbor(EAST);
}

void Chunk::unloadNeighbor(Direction dir)
{
	switch (dir) {
		case NORTH:
			_north = nullptr;
			break;
		case SOUTH:
			_south = nullptr;
			break;
		case EAST:
			_east = nullptr;
			break;
		case WEST:
			_west = nullptr;
			break;
		case DOWN:
			break;
		case UP:
			break;
	}
}

ivec2 Chunk::getPosition()
{
	return _position;
}

SubChunk *Chunk::getSubChunk(int y)
{
	if (_isInit == false)
		return nullptr;
	auto it = _subChunks.find(y);
	auto endIt = _subChunks.end();
	if (it != endIt)
		return it->second;
	return nullptr;
}

bool Chunk::isReady()
{
	return _facesSent;
}

void Chunk::clearFaces()
{
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
	// if (_facesSent == true)
	// 	return ;
	_sendFacesMutex.lock();
	clearFaces();
	// _subChunksMutex.lock();
	for (auto &subChunk : _subChunks)
	{
		if (!subChunk.second)
			continue ;
		subChunk.second->sendFacesToDisplay();
		std::vector<int> vertices = subChunk.second->getVertices();
		std::vector<int> transparentVertices = subChunk.second->getTransparentVertices();
		
		_indirectBufferData.push_back(DrawArraysIndirectCommand{
			4,
			uint(vertices.size()),
			0,
			uint(_vertexData.size()),
		});

		_transparentIndirectBufferData.push_back(DrawArraysIndirectCommand{
			4,
			uint(transparentVertices.size()),
			0,
			uint(_transparentVertexData.size()),
		});

		ivec3 pos = subChunk.second->getPosition();
		_ssboData.push_back(vec4{
			pos.x * CHUNK_SIZE, pos.y * CHUNK_SIZE, pos.z * CHUNK_SIZE, _resolution.load()
		});

		_vertexData.insert(_vertexData.end(), vertices.begin(), vertices.end());
		_transparentVertexData.insert(_transparentVertexData.end(), transparentVertices.begin(), transparentVertices.end());
	}
	_facesSent = true;
	// _subChunksMutex.unlock();
	_sendFacesMutex.unlock();
}

void Chunk::setNorthChunk(Chunk *chunk)
{
	_north = chunk;
	_hasAllNeighbors = _north && _south && _east && _west;
}

void Chunk::setSouthChunk(Chunk *chunk)
{
	_south = chunk;
	_hasAllNeighbors = _north && _south && _east && _west;
}

void Chunk::setEastChunk(Chunk *chunk)
{
	_east = chunk;
	_hasAllNeighbors = _north && _south && _east && _west;
}

void Chunk::setWestChunk(Chunk *chunk)
{
	_west = chunk;
	_hasAllNeighbors = _north && _south && _east && _west;
}

Chunk *Chunk::getNorthChunk() {
	return _north;
}

Chunk *Chunk::getSouthChunk() {
	return _south;
}

Chunk *Chunk::getEastChunk() {
	return _east;
}

Chunk *Chunk::getWestChunk() {
	return _west;
}

std::vector<int> &Chunk::getVertices()
{
	std::lock_guard<std::mutex> lock(_sendFacesMutex);
	return _vertexData;
}

std::vector<DrawArraysIndirectCommand> &Chunk::getIndirectData()
{
	std::lock_guard<std::mutex> lock(_sendFacesMutex);
	return _indirectBufferData;
}

std::vector<vec4> &Chunk::getSSBO()
{
	std::lock_guard<std::mutex> lock(_sendFacesMutex);
	return _ssboData;
}

std::vector<int> &Chunk::getTransparentVertices()
{
	std::lock_guard<std::mutex> lock(_sendFacesMutex);
	return _transparentVertexData;
}

std::vector<DrawArraysIndirectCommand> &Chunk::getTransparentIndirectData()
{
	std::lock_guard<std::mutex> lock(_sendFacesMutex);
	return _transparentIndirectBufferData;
}

void Chunk::freeSubChunks()
{
	_subChunksMutex.lock();
	for (auto &subchunk : _subChunks)
	{
		if (subchunk.second)
			delete subchunk.second;
		subchunk.second = nullptr;
	}
	_subChunksMutex.unlock();
	_subChunks.clear();
}

void	Chunk::updateResolution(int newResolution)
{
	_world.updatePerlinMapResolution(_perlinMap, newResolution);
	_resolution = newResolution;

	_subChunksMutex.lock();
	for (auto &subchunk : _subChunks)
	{
		SubChunk *subChunk = subchunk.second;
		subChunk->updateResolution(newResolution, _perlinMap);
	}
	_subChunksMutex.unlock();

	_facesSent = false;
	sendFacesToDisplay();
	if (_north)
		_north->sendFacesToDisplay();
	if (_south)
		_south->sendFacesToDisplay();
	if (_east)
		_east->sendFacesToDisplay();
	if (_west)
		_west->sendFacesToDisplay();
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
	if (minIdx == INT_MAX) { // fallback if somehow empty
		minp = { xs, 0.0f,  zs };
		maxp = { xs + CHUNK_SIZE, CHUNK_SIZE, zs + CHUNK_SIZE };
		return;
	}
	minp = { xs,				float(minIdx * CHUNK_SIZE), zs };
	maxp = { xs + CHUNK_SIZE,	float((maxIdx + 1) * CHUNK_SIZE), zs + CHUNK_SIZE };
}

std::atomic_int &Chunk::getResolution() {
	return _resolution;
}

void Chunk::snapshotDisplayData(
	std::vector<int>&							outSolidVerts,
	std::vector<DrawArraysIndirectCommand>&		outSolidCmds,
	std::vector<vec4>&							outSSBO,
	std::vector<int>&							outTranspVerts,
	std::vector<DrawArraysIndirectCommand>&		outTranspCmds)
{
	std::lock_guard<std::mutex> lockk(_sendFacesMutex);
	outSolidVerts   = _vertexData;
	outSolidCmds	= _indirectBufferData;
	outSSBO		 	= _ssboData;
	outTranspVerts  = _transparentVertexData;
	outTranspCmds   = _transparentIndirectBufferData;
}
