#include "Chunk.hpp"

Chunk::Chunk(vec2 pos, PerlinMap *perlinMap, World &world, TextureManager &textureManager) : _world(world), _textureManager(textureManager)
{
	_isInit = false;
	_perlinMap = perlinMap;
	for (int y = (perlinMap->lowest) - 64; y < (perlinMap->heighest) + CHUNK_SIZE; y += CHUNK_SIZE)
	{
		_subChunksMutex.lock();
		_subChunks[(y / CHUNK_SIZE)] = new SubChunk({pos.x, y / CHUNK_SIZE, pos.y}, perlinMap, *this, world, textureManager);
		_subChunksMutex.unlock();
	}
	_position = pos;
	_facesSent = false;
	sendFacesToDisplay();
	_isInit = true;
}

Chunk::~Chunk()
{
	_subChunksMutex.lock();
	for (auto &subchunk : _subChunks)
		delete subchunk.second;
	_subChunksMutex.unlock();
	_subChunks.clear();
}

void Chunk::getNeighbors()
{
	if (_isFullyLoaded)
		return ;
	// _chrono.startChrono(2, "Getting chunks");
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

	// _chrono.stopChrono(2);
    _isFullyLoaded = (_north && _south && _west && _east);
	_chrono.startChrono(3, "Sending faces");
    sendFacesToDisplay();
	_chrono.stopChrono(3);
	_chrono.printChronos();
}


vec2 Chunk::getPosition()
{
	return _position;
}

int Chunk::display()
{
	if (!_facesSent)
		return (0);
	int triangleDrawn = 0;
	_subChunksMutex.lock();
	for (auto &subchunk : _subChunks)
	{
		triangleDrawn += subchunk.second->display();
	}
	_subChunksMutex.unlock();
	return triangleDrawn;
}

SubChunk *Chunk::getSubChunk(int y)
{
	if (_isInit == false)
		return nullptr;
	_subChunksMutex.lock();
	auto it = _subChunks.find(y);
	auto endIt = _subChunks.end();
	_subChunksMutex.unlock();
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
	if (_facesSent)
		return ;
	_subChunksMutex.lock();
	for (auto &subChunk : _subChunks)
		subChunk.second->sendFacesToDisplay();
	_subChunksMutex.unlock();
	_facesSent = true;
}

void Chunk::setNorthChunk(Chunk *chunk)
{
	_north = chunk;

	_isFullyLoaded = (_north && _south && _west && _east);
	if (_isFullyLoaded)
	{
		_facesSent = false;
	}
	if (_isFullyLoaded)
		sendFacesToDisplay();
}

void Chunk::setSouthChunk(Chunk *chunk)
{
	_south = chunk;

	_isFullyLoaded = (_north && _south && _west && _east);
	if (_isFullyLoaded)
	{
		_facesSent = false;
	}
	if (_isFullyLoaded)
		sendFacesToDisplay();
}

void Chunk::setEastChunk(Chunk *chunk)
{
	_east = chunk;

	_isFullyLoaded = (_north && _south && _west && _east);
	if (_isFullyLoaded)
	{
		_facesSent = false;
	}
	if (_isFullyLoaded)
		sendFacesToDisplay();
}

void Chunk::setWestChunk(Chunk *chunk)
{
	_west = chunk;

	_isFullyLoaded = (_north && _south && _west && _east);
	if (_isFullyLoaded)
	{
		_facesSent = false;
	}
	if (_isFullyLoaded)
		sendFacesToDisplay();
}
