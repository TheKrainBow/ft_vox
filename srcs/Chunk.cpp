#include "Chunk.hpp"

Chunk::Chunk(vec2 position, PerlinMap *perlinMap, World &world, TextureManager &textureManager) : _world(world), _textureManager(textureManager)
{
	_isInit = false;
	_perlinMap = perlinMap;
	std::lock_guard<std::mutex> lock(_subChunksMutex);
	for (int y = (perlinMap->lowest) - 1 ; y < (perlinMap->heighest) + CHUNK_SIZE; y += CHUNK_SIZE)
	{
		SubChunk *newChunk = new SubChunk(vec3(position.x, y / CHUNK_SIZE, position.y), perlinMap, *this, world, textureManager);
		_subChunks.push_back(newChunk);
	}
	_position = position;
	_north = _south = _east = _west = nullptr;
	_isFullyLoaded = false;
	getNeighbors();
	_isInit = true;
}

Chunk::~Chunk()
{
	for (auto it = _subChunks.begin() ; it != _subChunks.end() ; it++)
		delete *it;
	_subChunks.clear();
}

void Chunk::getNeighbors()
{
	if (_isFullyLoaded)
		return ;
	if (!_north)
	{
		_north = _world.getChunk(vec2((int)_position.x + 1, (int)_position.y));
		if (_north)
			_north->setSouthChunk(this);
	}
	if (!_south)
	{
		_south = _world.getChunk(vec2((int)_position.x - 1, (int)_position.y));
		if (_south)
			_south->setNorthChunk(this);
	}
	if (!_east)
	{
		_east = _world.getChunk(vec2((int)_position.x, (int)_position.y + 1));
		if (_east)
			_east->setWestChunk(this);
	}
	if (!_west)
	{
		_west = _world.getChunk(vec2((int)_position.x, (int)_position.y - 1));
		if (_west)
			_west->setEastChunk(this);
	}
	_isFullyLoaded = (_north && _south && _west && _east);
	sendFacesToDisplay();
}

vec2 Chunk::getPosition()
{
	return _position;
}

int Chunk::display()
{
	if (!_isFullyLoaded)
		return (0);
	std::lock_guard<std::mutex> lock(_subChunksMutex);
	int triangleDrawn = 0;
	vec3 chunkPos;
	for (auto it = _subChunks.begin() ; it != _subChunks.end() ; it++)
	{
		chunkPos = (*it)->getPosition();
		triangleDrawn += (*it)->display();
	}
	return triangleDrawn;
}

SubChunk *Chunk::getSubChunk(int y)
{
	if (_isInit == false)
		return nullptr;
	vec3 chunkPos;
	for (auto it = _subChunks.begin() ; it != _subChunks.end() ; it++)
	{
		chunkPos = (*it)->getPosition();
		if (chunkPos.y == y)
			return (*it);
	}
	return nullptr;
}

void Chunk::sendFacesToDisplay()
{
	if (!_isFullyLoaded)
		return ;
	std::lock_guard<std::mutex> lock(_subChunksMutex);
	for (auto subChunk : _subChunks)
		subChunk->sendFacesToDisplay();
}

Chunk *Chunk::getNorthChunk()
{
	return _north;
}

Chunk *Chunk::getSouthChunk()
{
	return _south;
}

Chunk *Chunk::getEastChunk()
{
	return _east;
}

Chunk *Chunk::getWestChunk()
{
	return _west;
}

void Chunk::setNorthChunk(Chunk *chunk)
{
	_north = chunk;
	_isFullyLoaded = (_north && _south && _west && _east);
	if (_isFullyLoaded)
		sendFacesToDisplay();
}

void Chunk::setSouthChunk(Chunk *chunk)
{
	_south = chunk;
	_isFullyLoaded = (_north && _south && _west && _east);
	if (_isFullyLoaded)
		sendFacesToDisplay();
}

void Chunk::setEastChunk(Chunk *chunk)
{
	_east = chunk;
	_isFullyLoaded = (_north && _south && _west && _east);
	if (_isFullyLoaded)
		sendFacesToDisplay();
}

void Chunk::setWestChunk(Chunk *chunk)
{
	_west = chunk;
	_isFullyLoaded = (_north && _south && _west && _east);
	if (_isFullyLoaded)
		sendFacesToDisplay();
}
