#include "Chunk.hpp"

Chunk::Chunk(ivec2 pos, PerlinMap *perlinMap, World &world, TextureManager &textureManager) : _world(world), _textureManager(textureManager)
{
	_isInit = false;
	_perlinMap = perlinMap;
	_position = pos;
	_facesSent = false;
	_needUpdate = true;
	_hasBufferInitialized = false;
	_hasAllNeighbors = false;
	int heighest = perlinMap->heighest;
	int lowest = perlinMap->lowest;
	if (heighest < OCEAN_HEIGHT) {
		heighest = OCEAN_HEIGHT;
	}
	heighest = heighest / CHUNK_SIZE * CHUNK_SIZE;
	lowest = lowest / CHUNK_SIZE * CHUNK_SIZE;
	for (int y = (lowest) - (CHUNK_SIZE); y < (heighest) + (CHUNK_SIZE * 2); y += CHUNK_SIZE)
	{
		_subChunks[y / CHUNK_SIZE] = new SubChunk({pos.x, int(y / CHUNK_SIZE), pos.y}, perlinMap, *this, world, textureManager);
	}
	_isInit = true;
	// sendFacesToDisplay();
	getNeighbors();
}

Chunk::~Chunk()
{
	for (auto &subchunk : _subChunks)
		delete subchunk.second;
	_subChunks.clear();
}

void Chunk::clearFaces() {
	_vertexData.clear();
	_ssboData.clear();
	_indirectBufferData.clear();
	// _transparentVertexData.clear();
	// _hasSentFaces = false;
	_needUpdate = true;
	// _needTransparentUpdate = true;
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

void Chunk::sendFacesToDisplay()
{
	if (_hasAllNeighbors == false)
		return ;
	if (_facesSent == true)
		return ;
	_sendFacesMutex.lock();
		// clearFaces();
		// return ;
	for (auto &subChunk : _subChunks)
	{
		subChunk.second->sendFacesToDisplay();
		std::vector<int> vertices = subChunk.second->getVertices();
		_indirectBufferData.push_back(DrawArraysIndirectCommand{
			4,
			uint(vertices.size()),
			0,
			uint(_vertexData.size()),
		});
		ivec3 pos = subChunk.second->getPosition();
		_ssboData.push_back(ivec4{
			pos.x * CHUNK_SIZE, pos.y * CHUNK_SIZE, pos.z * CHUNK_SIZE, 0
		});
		_vertexData.insert(_vertexData.end(), vertices.begin(), vertices.end());
	}
	_facesSent = true;
	_sendFacesMutex.unlock();
}

void Chunk::setNorthChunk(Chunk *chunk)
{
	_north = chunk;
	if (_north && _south && _east && _west)
		_hasAllNeighbors = true;
}

void Chunk::setSouthChunk(Chunk *chunk)
{
	_south = chunk;
	if (_north && _south && _east && _west)
		_hasAllNeighbors = true;
}

void Chunk::setEastChunk(Chunk *chunk)
{
	_east = chunk;
	if (_north && _south && _east && _west)
		_hasAllNeighbors = true;
}

void Chunk::setWestChunk(Chunk *chunk)
{
	_west = chunk;
	if (_north && _south && _east && _west)
		_hasAllNeighbors = true;
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
