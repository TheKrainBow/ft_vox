#include "World.hpp"
#include "define.hpp"
#include <cmath>
#include <iostream>
#include <memory>
#include <map>
#include <tuple>
#include <iostream>
#include <future>
#include <thread>

// Helper function to calculate block position within a chunk
vec3 World::calculateBlockPos(vec3 position) const {
	auto mod = [](int value) { return (value >= 0) ? value % CHUNK_SIZE : (CHUNK_SIZE + (value % CHUNK_SIZE)) % CHUNK_SIZE; };
	return { mod(position.x), mod(position.y), mod(position.z) };
}

World::World(int seed, TextureManager &textureManager, Camera &camera) : _perlinGenerator(seed), _textureManager(textureManager), _camera(&camera) {
	_displayedChunk = new Chunk*[_maxRender * _maxRender];
	bzero(_displayedChunk, _maxRender * _maxRender);
	_renderDistance = RENDER_DISTANCE;
}

World::~World() {
	std::lock_guard<std::mutex> lock(_chunksMutex);
	for (auto it = _chunks.begin(); it != _chunks.end(); it++)
		delete it->second;
	delete [] _displayedChunk;
}

NoiseGenerator &World::getNoiseGenerator(void)
{
	return (_perlinGenerator);
}


void World::loadPerlinMap(vec3 camPosition)
{
	_perlinGenerator.clearPerlinMaps();
	vec2 position;
	for (int x = -RENDER_DISTANCE; x < RENDER_DISTANCE; x++)
	{
		for (int z = -RENDER_DISTANCE; z < RENDER_DISTANCE; z++)
		{
			position.x = trunc(camPosition.x / CHUNK_SIZE) + x;
			position.y = trunc(camPosition.z / CHUNK_SIZE) + z;
			_perlinGenerator.addPerlinMap(position.x, position.y, CHUNK_SIZE, 1);
		}
	}
}

int *World::getRenderDistancePtr()
{
	return &_renderDistance;
}

void World::updateNeighbours(std::pair<int, int> pair)
{
	std::pair<int, int> top (pair.first + 1, pair.second);
	std::pair<int, int> bot (pair.first - 1, pair.second);
	std::pair<int, int> left (pair.first, pair.second + 1);
	std::pair<int, int> right (pair.first, pair.second - 1);

	auto it = _chunks.find(top);
	if (it != _chunks.end())
		it->second->sendFacesToDisplay();
	it = _chunks.find(bot);
	if (it != _chunks.end())
		it->second->sendFacesToDisplay();
	it = _chunks.find(left);
	if (it != _chunks.end())
		it->second->sendFacesToDisplay();
	it = _chunks.find(right);
	if (it != _chunks.end())
		it->second->sendFacesToDisplay();
}

void World::loadChunk(int x, int z, int renderMax, int currentRender, vec3 camPosition)
{
	Chunk *chunk = nullptr;
	int correctX = (renderMax / 2) - (currentRender / 2) + x;
	int correctZ = (renderMax / 2) - (currentRender / 2) + z;
	std::pair<int, int> pair(camPosition.x / CHUNK_SIZE - currentRender / 2 + x ,camPosition.z / CHUNK_SIZE - currentRender / 2 + z);
	
	_chunksMutex.lock();
	auto it = _chunks.find(pair);
	auto itend = _chunks.end();
	if (it != itend)
		chunk = it->second;
	else if (_skipLoad == false)
	{
		_chunksMutex.unlock();
		chunk = new Chunk(vec2(pair.first, pair.second), _perlinGenerator.getPerlinMap(pair.first, pair.second), *this, _textureManager);
		_chunksMutex.lock();
		it = _chunks.find(pair);
		itend = _chunks.end();
		if (it == itend)
			_chunks[pair] = chunk;
		else
		{
			delete chunk;
			chunk = nullptr;
		}
	}
	_chunksMutex.unlock();
	_displayMutex.lock();
	_displayedChunk[correctX + correctZ * renderMax] = chunk;
	_displayMutex.unlock();
}

int World::loadTopChunks(int renderDistance, int render, vec3 camPosition)
{
	for (int x = 1; x < render; x++)
	{
		int z = 0;
		loadChunk(x, z, renderDistance, render, camPosition);
	}
	return 1;
}

int World::loadBotChunks(int renderDistance, int render, vec3 camPosition)
{
	for (int x = render - 2; x >= 0; x--)
	{
		int z = render - 1;
		loadChunk(x, z, renderDistance, render, camPosition);
	}
	return 1;
}

int World::loadRightChunks(int renderDistance, int render, vec3 camPosition)
{
	for (int z = 1; z < render; z++)
	{
		int x = render - 1;
		loadChunk(x, z, renderDistance, render, camPosition);
	}
	return 1;
}

int World::loadLeftChunks(int renderDistance, int render, vec3 camPosition)
{
	for (int z = render - 2; z >= 0; z--)
	{
		int x = 0;
		loadChunk(x, z, renderDistance, render, camPosition);
	}
	return 1;
}

void World::setRunning(std::mutex *runningMutex, bool *isRunning)
{
	_isRunning = isRunning;
	_runningMutex = runningMutex;
}

bool World::getIsRunning()
{
	std::lock_guard<std::mutex> lockGuard(*_runningMutex);
	return *_isRunning;
}

void World::loadChunks(vec3 camPosition)
{
	int renderDistance = _renderDistance;
	std::future<int> retTop;
	std::future<int> retBot;
	std::future<int> retLeft;
	std::future<int> retRight;

	vec3 oldCamChunk = vec3(camPosition.x / CHUNK_SIZE, camPosition.y / CHUNK_SIZE, camPosition.z / CHUNK_SIZE);
	if (oldCamChunk.x < 0) oldCamChunk.x--;
	if (oldCamChunk.y < 0) oldCamChunk.y--;
	if (oldCamChunk.z < 0) oldCamChunk.z--;

	_skipLoad = false;
	loadChunk(0, 0, renderDistance, 1, camPosition);
	for (int render = 1; getIsRunning() && render < renderDistance; render += 2)
	{
		retTop = std::async(std::launch::async, 
			std::bind(&World::loadTopChunks, this, renderDistance, render, camPosition));
		retTop.get();
		retBot = std::async(std::launch::async, 
			std::bind(&World::loadBotChunks, this, renderDistance, render, camPosition));
		retBot.get();
		retRight = std::async(std::launch::async, 
			std::bind(&World::loadRightChunks, this, renderDistance, render, camPosition));
		retRight.get();
		retLeft = std::async(std::launch::async, 
			std::bind(&World::loadLeftChunks, this, renderDistance, render, camPosition));
		retLeft.get();
		vec3 newPos = _camera->getWorldPosition();
		vec3 camChunk(newPos.x / CHUNK_SIZE, newPos.y / CHUNK_SIZE, newPos.z / CHUNK_SIZE);
		if (newPos.x < 0) camChunk.x--;
		if (newPos.y < 0) camChunk.y--;
		if (newPos.z < 0) camChunk.z--;

		if (((floor(oldCamChunk.x) != floor(camChunk.x) || floor(oldCamChunk.z) != floor(camChunk.z))))
			break ;
	}
}

char World::getBlock(vec3 position)
{
	// std::cout << "World::getBlock" << std::endl;
	vec3 chunkPos(position);
	chunkPos /= CHUNK_SIZE;
	chunkPos.x -= (position.x < 0 && abs((int)position.x) % CHUNK_SIZE != 0);
	chunkPos.z -= (position.z < 0 && abs((int)position.z) % CHUNK_SIZE != 0);
	chunkPos.y -= (position.y < 0 && abs((int)position.y) % CHUNK_SIZE != 0);

	SubChunk *chunk = getSubChunk(chunkPos);
	if (!chunk)
		return NOT_FOUND;
	return chunk->getBlock(calculateBlockPos(position));
}

SubChunk *World::getSubChunk(vec3 position)
{
	_chunksMutex.lock();
	auto it = _chunks.find(std::make_pair(position.x, position.z));
	auto itend = _chunks.end();
	_chunksMutex.unlock();
	if (it != itend)
		return it->second->getSubChunk(position.y);
	return nullptr;
}

Chunk *World::getChunk(vec2 position)
{
	_chunksMutex.lock();
	auto it = _chunks.find(std::make_pair(position.x, position.y));
	auto itend = _chunks.end();
	_chunksMutex.unlock();
	if (it != itend)
	{
		return it->second;
	}
	return nullptr;
}

int World::display()
{
	int triangleDrawn = 0;
	Chunk *chunkToDisplay;
	for (int x = 0; x < _renderDistance; x++)
	{
		for (int z = 0; z < _renderDistance; z++)
		{
			_displayMutex.lock();
			chunkToDisplay = _displayedChunk[x + z * _renderDistance];
			_displayMutex.unlock();
			if (chunkToDisplay)
			{
				triangleDrawn += chunkToDisplay->display();
			}
		}
	}
	return (triangleDrawn);
}

int	World::getCachedChunksNumber()
{
	return _chunks.size();
}

void World::increaseRenderDistance()
{
	_renderDistance += 2;
	if (_renderDistance > _maxRender)
		_renderDistance = _maxRender;
}

void World::decreaseRenderDistance()
{
	_renderDistance -= 2;
	if (_renderDistance < 1)
		_renderDistance = 1;
}