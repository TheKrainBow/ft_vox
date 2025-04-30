#include "Chunk.hpp"

Chunk::Chunk(ivec2 pos, PerlinMap *perlinMap, World &world, TextureManager &textureManager, int resolution)
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
	for (int y = (lowest) - (CHUNK_SIZE); y < (heighest) + (CHUNK_SIZE * 2); y += CHUNK_SIZE)
	{
		int index = y / CHUNK_SIZE;
		SubChunk *subChunk = _subChunks[index] = new SubChunk({_position.x, index, _position.y}, _perlinMap, *this, _world, _textureManager, _resolution);
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

Chunk::~Chunk()
{
	for (auto &subchunk : _subChunks)
		delete subchunk.second;
	_subChunks.clear();
}

void Chunk::getNeighbors()
{
	_north = _world.getChunk({_position.x, _position.y - 1});
	_south = _world.getChunk({_position.x, _position.y + 1});
	_east = _world.getChunk({_position.x + 1, _position.y});
	_west = _world.getChunk({_position.x - 1, _position.y});

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
	if (_hasAllNeighbors == false)
		return ;
	// if (_facesSent == true)
	// 	return ;
	_sendFacesMutex.lock();
	clearFaces();
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
		delete subchunk.second;
	_subChunksMutex.unlock();
	_subChunks.clear();
}

void	Chunk::updateResolution(int newResolution, Direction dir)
{
	(void)dir;
	_world._perlinGenerator.updatePerlinMapResolution(_perlinMap, newResolution);
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
