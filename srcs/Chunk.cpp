#include "Chunk.hpp"

Chunk::Chunk(ivec2 pos, PerlinMap *perlinMap, CaveGenerator &caveGen, World &world, TextureManager &textureManager, int resolution)
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
_resolution(resolution)
{
}

void Chunk::loadBlocks()
{
	int heighest = _perlinMap->heighest;
	int lowest = _perlinMap->lowest;
	if (heighest < OCEAN_HEIGHT) {
		heighest = OCEAN_HEIGHT;
	}
	heighest = heighest / CHUNK_SIZE * CHUNK_SIZE;
	lowest = lowest / CHUNK_SIZE * CHUNK_SIZE;
	lowest = 0;
	for (int y = (lowest) - (CHUNK_SIZE); y < (heighest) + (CHUNK_SIZE * 2 + (_resolution == CHUNK_SIZE)); y += CHUNK_SIZE)
	{
		int index = y / CHUNK_SIZE;
		SubChunk *subChunk = _subChunks[index] = new SubChunk({_position.x, index, _position.y}, _perlinMap, _caveGen, *this, _world, _textureManager, _resolution);
		subChunk->loadHeight(0);
		subChunk->loadBiome(0);
		_memorySize += subChunk->getMemorySize();
	}
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

TopBlock Chunk::getTopBlockUnderPlayer(int localX, int localY, int localZ)
{
	std::lock_guard<std::mutex> lock(_subChunksMutex);
	int index = std::numeric_limits<int>::min();
	(void)localY;

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
    minp = { xs,              float(minIdx * CHUNK_SIZE), zs };
    maxp = { xs + CHUNK_SIZE, float((maxIdx + 1) * CHUNK_SIZE), zs + CHUNK_SIZE };
}
