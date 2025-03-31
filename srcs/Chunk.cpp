#include "Chunk.hpp"

Chunk::Chunk(vec2 pos, PerlinMap *perlinMap, World &world, TextureManager &textureManager) : _world(world), _textureManager(textureManager)
{
	_isInit = false;
	_perlinMap = perlinMap;
	for (int y = (perlinMap->lowest) - (CHUNK_SIZE * 2); y < (perlinMap->heighest) + (CHUNK_SIZE * 2); y += CHUNK_SIZE)
	{
		_subChunks[y / CHUNK_SIZE] = new SubChunk({pos.x, int(y / CHUNK_SIZE), pos.y}, perlinMap, *this, world, textureManager);
	}
	_isInit = true;
	_position = pos;
	_facesSent = false;
	// getNeighbors();
	sendFacesToDisplay();
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
	// if (_isFullyLoaded)
	// 	return ;
	// _chrono.startChrono(2, "Getting chunks");
    // std::future<Chunk *> retNorth = std::async(std::launch::async, [this]() {
    //     return _world.getChunk({_position.x + 1, _position.y});
    // });

    // std::future<Chunk *> retSouth = std::async(std::launch::async, [this]() {
    //     return _world.getChunk({_position.x - 1, _position.y});
    // });

    // std::future<Chunk *> retEast = std::async(std::launch::async, [this]() {
    //     return _world.getChunk({_position.x, _position.y + 1});
    // });

    // std::future<Chunk *> retWest = std::async(std::launch::async, [this]() {
    //     return _world.getChunk({_position.x, _position.y - 1});
    // });

    // Wait for all futures and assign the results
    _north = _world.getChunk({_position.x + 1, _position.y});
    _south = _world.getChunk({_position.x - 1, _position.y});
    _east = _world.getChunk({_position.x, _position.y + 1});
    _west = _world.getChunk({_position.x, _position.y - 1});
    if (_north) {
		_north->setSouthChunk(this);
		// _north->sendFacesToDisplay();
	}

    // _south = retSouth.get();
    if (_south) {
		_south->setNorthChunk(this);
		// _south->sendFacesToDisplay();
	}

    // _east = retEast.get();
    if (_east) {
		_east->setNorthChunk(this);
		// _east->sendFacesToDisplay();
	}

    // _west = retWest.get();
    if (_west) {
		_west->setNorthChunk(this);
		// _west->sendFacesToDisplay();
	}

	// _chrono.stopChrono(2);
	// _chrono.startChrono(3, "Sending faces");
    sendFacesToDisplay();
	// _chrono.stopChrono(3);
	// _chrono.printChronos();
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
	for (auto &subchunk : _subChunks)
	{
		triangleDrawn += subchunk.second->display();
	}
	return triangleDrawn;
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
	if (_facesSent)
		return ;
	for (auto &subChunk : _subChunks)
		subChunk.second->sendFacesToDisplay();
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
