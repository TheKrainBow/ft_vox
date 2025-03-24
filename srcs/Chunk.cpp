#include "Chunk.hpp"

Chunk::Chunk(vec2 pos, PerlinMap *perlinMap, World &world, TextureManager &textureManager, bool isBorder) : _world(world), _textureManager(textureManager)
{
	_isInit = false;
	_perlinMap = perlinMap;
	for (int y = (perlinMap->lowest) - 1; y < (perlinMap->heighest) + CHUNK_SIZE; y += CHUNK_SIZE)
	{
		_subChunksMutex.lock();
		_subChunks.emplace_back(new SubChunk({pos.x, y / CHUNK_SIZE, pos.y}, perlinMap, *this, world, textureManager));
		_subChunksMutex.unlock();
	}
	_position = pos;
	_north = _south = _east = _west = nullptr;
	_isFullyLoaded = false;
	_facesSent = false;
	_isBorder = isBorder;
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
	// chrono.startChrono(2, "Getting chunks");
    std::future<Chunk *> retNorth = std::async(std::launch::async, [this]() {
        return _world.getChunk({_position.x + 1, _position.y});
    });

    std::future<Chunk *> retSouth = std::async(std::launch::async, [this]() {
        return _world.getChunk({_position.x - 1, _position.y});
    });

    std::future<Chunk *> retEast = std::async(std::launch::async, [this]() {
        return _world.getChunk({_position.x, _position.y + 1});
    });

    std::future<Chunk *> retWest = std::async(std::launch::async, [this]() {
        return _world.getChunk({_position.x, _position.y - 1});
    });

    // Wait for all futures and assign the results
    _north = retNorth.get();
    if (_north) _north->setSouthChunk(this);

    _south = retSouth.get();
    if (_south) _south->setNorthChunk(this);

    _east = retEast.get();
    if (_east) _east->setWestChunk(this);

    _west = retWest.get();
    if (_west) _west->setEastChunk(this);

	// chrono.stopChrono(2);
    _isFullyLoaded = (_north && _south && _west && _east);
	chrono.startChrono(3, "Sending faces");
    sendFacesToDisplay();
	chrono.stopChrono(3);
	chrono.printChronos();
}


vec2 Chunk::getPosition()
{
	return _position;
}

int Chunk::display()
{
	if (!_isFullyLoaded && !_isBorder)
		return (0);
	std::lock_guard<std::mutex> lock(_subChunksMutex);
	int triangleDrawn = 0;
	vec3 chunkPos;
	for (auto it = _subChunks.rbegin() ; it != _subChunks.rend() ; it++)
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

bool Chunk::isReady()
{
	return (_isFullyLoaded || _isBorder) && _facesSent;
}

void Chunk::sendFacesToDisplay()
{
	if (_facesSent)
		return ;
	std::lock_guard<std::mutex> lock(_subChunksMutex);
	for (auto &subChunk : _subChunks)
		subChunk->sendFacesToDisplay();
	_facesSent = true;
}

void Chunk::setNorthChunk(Chunk *chunk)
{
	_north = chunk;

	_isFullyLoaded = (_north && _south && _west && _east);
	if (_isFullyLoaded && _isBorder)
	{
		_isBorder = false;
		_facesSent = false;
	}
	if (_isFullyLoaded)
		sendFacesToDisplay();
}

void Chunk::setSouthChunk(Chunk *chunk)
{
	_south = chunk;

	_isFullyLoaded = (_north && _south && _west && _east);
	if (_isFullyLoaded && _isBorder)
	{
		_isBorder = false;
		_facesSent = false;
	}
	if (_isFullyLoaded)
		sendFacesToDisplay();
}

void Chunk::setEastChunk(Chunk *chunk)
{
	_east = chunk;

	_isFullyLoaded = (_north && _south && _west && _east);
	if (_isFullyLoaded && _isBorder)
	{
		_isBorder = false;
		_facesSent = false;
	}
	if (_isFullyLoaded)
		sendFacesToDisplay();
}

void Chunk::setWestChunk(Chunk *chunk)
{
	_west = chunk;

	_isFullyLoaded = (_north && _south && _west && _east);
	if (_isFullyLoaded && _isBorder)
	{
		_isBorder = false;
		_facesSent = false;
	}
	if (_isFullyLoaded)
		sendFacesToDisplay();
}
