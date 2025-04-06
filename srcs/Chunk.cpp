#include "Chunk.hpp"

Chunk::Chunk(ivec2 pos, PerlinMap *perlinMap, World &world, TextureManager &textureManager) : _world(world), _textureManager(textureManager)
{
	_isInit = false;
	_perlinMap = perlinMap;
	_position = pos;
	_facesSent = false;
	getNeighbors();
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
	sendFacesToDisplay();

    if (_north) {
		_north->sendFacesToDisplay();
	}

    // _south = retSouth.get();
    if (_south) {
		_south->sendFacesToDisplay();
	}

    // _east = retEast.get();
    if (_east) {
		_east->sendFacesToDisplay();
	}

    // _west = retWest.get();
    if (_west) {
		_west->sendFacesToDisplay();
	}
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
    _north = _world.getChunk({_position.x, _position.y - 1});
    _south = _world.getChunk({_position.x, _position.y + 1});
    _east = _world.getChunk({_position.x + 1, _position.y});
    _west = _world.getChunk({_position.x - 1, _position.y});
    if (_north) {
		_north->setSouthChunk(this);
	}

    // _south = retSouth.get();
    if (_south) {
		_south->setNorthChunk(this);
	}

    // _east = retEast.get();
    if (_east) {
		_east->setWestChunk(this);
	}

    // _west = retWest.get();
    if (_west) {
		_west->setEastChunk(this);
	}

	// _chrono.stopChrono(2);
	// _chrono.startChrono(3, "Sending faces");
	// _chrono.stopChrono(3);
	// _chrono.printChronos();
}


ivec2 Chunk::getPosition()
{
	return _position;
}

int Chunk::displayTransparent()
{
	if (!_facesSent)
		return (0);
	int triangleDrawn = 0;
	for (auto &subchunk : _subChunks)
	{
		_subChunksMutex.lock();
		triangleDrawn += subchunk.second->displayTransparent();
		_subChunksMutex.unlock();
	}
	return triangleDrawn;
}

int Chunk::display()
{
	if (!_facesSent)
		return (0);
	int triangleDrawn = 0;
	for (auto &subchunk : _subChunks)
	{
		_subChunksMutex.lock();
		triangleDrawn += subchunk.second->display();
		_subChunksMutex.unlock();
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
	_subChunksMutex.lock();
	for (auto &subChunk : _subChunks)
		subChunk.second->sendFacesToDisplay();
	_subChunksMutex.unlock();
	_facesSent = true;
	_loads++;
}

void Chunk::setNorthChunk(Chunk *chunk)
{
	_north = chunk;
}

void Chunk::setSouthChunk(Chunk *chunk)
{
	_south = chunk;
}

void Chunk::setEastChunk(Chunk *chunk)
{
	_east = chunk;
}

void Chunk::setWestChunk(Chunk *chunk)
{
	_west = chunk;
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


// Load chunks: 0,203s
// Load chunks: 0,256s
// Load chunks: 0,245s
// Load chunks: 0,235s
// Load chunks: 0,208s