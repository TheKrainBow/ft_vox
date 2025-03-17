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
vec3 World::calculateBlockPos(vec3 position) const
{
	auto mod = [](int value) { return (value >= 0) ? value % CHUNK_SIZE : (CHUNK_SIZE + (value % CHUNK_SIZE)) % CHUNK_SIZE; };
	return { mod(position.x), mod(position.y), mod(position.z) };
}

World::World(int seed, TextureManager &textureManager, Camera &camera) : _perlinGenerator(seed), _textureManager(textureManager), _camera(&camera)
{
	_displayedChunk = new ChunkSlot[_maxRender * _maxRender];

	for (int i = 0; i < _maxRender * _maxRender; ++i)
		_displayedChunk[i].chunk = nullptr;
	
	_renderDistance = RENDER_DISTANCE;
	generateSpiralOrder();
}

World::~World()
{
	std::lock_guard<std::mutex> lock(_chunksMutex);
	for (auto it = _chunks.begin(); it != _chunks.end(); it++)
		delete it->second;
	_displayMutex.lock();
	delete [] _displayedChunk;
	_displayMutex.unlock();
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

void World::unloadChunk()
{
	_chunksListMutex.lock();
	size_t listSize = _chunkList.size();
	_chunksListMutex.unlock();
// TODO: Unload perlin maps too/Fix segfault when coming back to removed terrain
	Chunk *tempChunk = nullptr;
	if (listSize >= CACHE_SIZE)
	{
		_chunksListMutex.lock();
		auto listIt = _chunkList.begin();
		auto listItend = _chunkList.end();
		if (listIt != listItend)
			tempChunk = (*listIt);
		_chunksListMutex.unlock();
		if (!tempChunk)
			return ;
		vec2 pos = tempChunk->getPosition();
		std::pair<int, int> chunkPair(pos.x, pos.y);

		_chunksMutex.lock();
		auto chunksIt = _chunks.find(chunkPair);
		auto chunksItend = _chunks.end();

		if (chunksIt == chunksItend)
			return _chunksMutex.unlock();
		_chunks.erase(chunksIt);
		_chunksMutex.unlock();

		_chunksListMutex.lock();
		_chunkList.erase(listIt);
		_chunksListMutex.unlock();
		_displayMutex.lock();
		for (int i = 0; i < _maxRender * _maxRender; ++i)
		{
			_displayedChunk[i].mutex.lock();
			if (_displayedChunk[i].chunk)
			{
				if (_displayedChunk[i].chunk == tempChunk)
				{
					_displayedChunk[i].chunk = nullptr;
					delete tempChunk;
					_displayedChunk[i].mutex.unlock();
					_displayMutex.unlock();
					return ;
				}
			}
			_displayedChunk[i].mutex.unlock();
		}
		_displayMutex.unlock();
		delete tempChunk;
	}
}

void World::loadChunk(int x, int z, int renderDistance, int render, vec3 camPosition)
{
	Chunk *chunk = nullptr;
	int correctX = (renderDistance / 2) - (render / 2) + x;
	int correctZ = (renderDistance / 2) - (render / 2) + z;
	std::pair<int, int> pair(int(camPosition.x) / CHUNK_SIZE - render / 2 + x , int(camPosition.z) / CHUNK_SIZE - render / 2 + z);

	_chunksMutex.lock();
	auto it = _chunks.find(pair);
	auto itend = _chunks.end();
	if (it != itend)
	{
		chunk = it->second;
		_chunksMutex.unlock();
	}
	else if (_skipLoad == false)
	{
		_chunksMutex.unlock();
		if (render == renderDistance - 1)
			chunk = new Chunk(vec2(pair.first, pair.second), _perlinGenerator.getPerlinMap(pair.first, pair.second), *this, _textureManager, true);
		else
			chunk = new Chunk(vec2(pair.first, pair.second), _perlinGenerator.getPerlinMap(pair.first, pair.second), *this, _textureManager, false);
		
		_chunksListMutex.lock();
		_chunkList.emplace_back(chunk);
		_chunksListMutex.unlock();

		_chunksMutex.lock();
		_chunks[pair] = chunk;
		_chunksMutex.unlock();
	}
	_displayedChunk[correctX + correctZ * renderDistance].mutex.lock();
	_displayedChunk[correctX + correctZ * renderDistance].chunk = chunk;
	_displayedChunk[correctX + correctZ * renderDistance].mutex.unlock();
	// unloadChunk();
}

int World::loadTopChunks(int renderDistance, int render, vec3 camPosition)
{
	int z = 0;
	for (int x = 1; x < render; x++)
	{
		loadChunk(x, z, renderDistance, render, camPosition);
	}
	return 1;
}

int World::loadBotChunks(int renderDistance, int render, vec3 camPosition)
{
	int z = render - 1;
	for (int x = render - 2; x >= 0; x--)
	{
		loadChunk(x, z, renderDistance, render, camPosition);
	}
	return 1;
}

int World::loadRightChunks(int renderDistance, int render, vec3 camPosition)
{
	int x = render - 1;
	for (int z = 1; z < render; z++)
	{
		loadChunk(x, z, renderDistance, render, camPosition);
	}
	return 1;
}

int World::loadLeftChunks(int renderDistance, int render, vec3 camPosition)
{
	int x = 0;
	for (int z = render - 2; z >= 0; z--)
	{
		loadChunk(x, z, renderDistance, render, camPosition);
	}
	return 1;
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
	
	resetTerrain();
	loadChunk(0, 0, renderDistance, 1, camPosition);
	for (int render = 2; getIsRunning() && render <= renderDistance; render += 2)
	{
		retTop = std::async(std::launch::async, 
			std::bind(&World::loadTopChunks, this, renderDistance, render, camPosition));
		retBot = std::async(std::launch::async, 
			std::bind(&World::loadBotChunks, this, renderDistance, render, camPosition));
		retTop.get();
		retBot.get();
		retRight = std::async(std::launch::async, 
			std::bind(&World::loadRightChunks, this, renderDistance, render, camPosition));
		retLeft = std::async(std::launch::async, 
			std::bind(&World::loadLeftChunks, this, renderDistance, render, camPosition));
		retRight.get();
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
	chunkPos.x = int(chunkPos.x);
	chunkPos.y = int(chunkPos.y);
	chunkPos.z = int(chunkPos.z);

	SubChunk *chunk = getSubChunk(chunkPos);
	if (!chunk)
	{
		//std::cout << "Couldn't get Subchunk in: " << chunkPos.x << " " << chunkPos.y << " " << chunkPos.z << std::endl;
		return NOT_FOUND;
	}
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
	//std::cout << "Couldn't get Chunk in: " << position.x << " " << position.y << " " << position.z << std::endl;
	return nullptr;
}

Chunk *World::getChunk(vec2 position)
{
	_chunksMutex.lock();
	auto it = _chunks.find(std::make_pair((int)position.x, (int)position.y));
	auto itend = _chunks.end();
	_chunksMutex.unlock();
	if (it != itend)
	{
		return it->second;
	}
	return nullptr;
}

void World::generateSpiralOrder()
{
	int renderDistance = _renderDistance - 1;
    int centerX = renderDistance / 2;
    int centerZ = renderDistance / 2;

    _spiralOrder.clear();
    _spiralOrder.reserve(renderDistance * renderDistance);

    int x = centerX;
    int z = centerZ;

    _spiralOrder.emplace_back(x, z);

    int dx = 1; // Start by going right
    int dz = 0;

    int steps = 1;
    int stepCount = 0;
    int directionChanges = 0;

    while (_spiralOrder.size() < renderDistance * renderDistance)
    {
        // Move to the next position
        x += dx;
        z += dz;

        // If within bounds, add to the spiral order
        if (x >= 0 && x < renderDistance && z >= 0 && z < renderDistance)
        {
            _spiralOrder.emplace_back(x, z);
        }

        stepCount++;

        // If we've completed the current segment length, change direction
        if (stepCount == steps)
        {
            stepCount = 0;
            directionChanges++;

            // Change direction (Right → Down → Left → Up)
            if (dx == 1 && dz == 0) { dx = 0; dz = 1; }   // Right → Down
            else if (dx == 0 && dz == 1) { dx = -1; dz = 0; }  // Down → Left
            else if (dx == -1 && dz == 0) { dx = 0; dz = -1; }  // Left → Up
            else if (dx == 0 && dz == -1) { dx = 1; dz = 0; } // Up → Right

            // Increase segment length every two turns
            if (directionChanges % 2 == 0)
            {
                steps++;
            }
        }
    }
}

void World::resetTerrain()
{
	for (auto [x, z] : _spiralOrder)
	{
		_displayMutex.lock();
		std::lock_guard<std::mutex> lock(_displayedChunk[x + z * _renderDistance].mutex);
		_displayedChunk[x + z * _renderDistance].chunk = nullptr;
		_displayMutex.unlock();
	}
}

int World::display()
{
	int triangleDrawn = 0;
	std::vector<std::pair<int, int>> retryChunks;
	int retryCount = 0;

	bool allChunksReady = false;

	while (!allChunksReady)
	{
		allChunksReady = true;
		retryCount++;

		for (auto [x, z] : _spiralOrder)
		{
			Chunk* chunkToDisplay = nullptr;
			_displayMutex.lock();
			std::lock_guard<std::mutex> lock(_displayedChunk[x + z * _renderDistance].mutex);
			chunkToDisplay = _displayedChunk[x + z * _renderDistance].chunk;
			_displayMutex.unlock();

			if (chunkToDisplay && chunkToDisplay->isReady())
			{
				triangleDrawn += chunkToDisplay->display();
			}
			else
			{
				retryChunks.emplace_back(x, z);
				allChunksReady = false;
			}
		}

		if (!allChunksReady)
		{
			for (auto [x, z] : retryChunks)
			{
				Chunk* chunkToDisplay = nullptr;
				_displayMutex.lock();
				std::lock_guard<std::mutex> lock(_displayedChunk[x + z * _renderDistance].mutex);
				chunkToDisplay = _displayedChunk[x + z * _renderDistance].chunk;
				_displayMutex.unlock();

				if (chunkToDisplay && chunkToDisplay->isReady())
				{
					triangleDrawn += chunkToDisplay->display();
				}
			}

			retryChunks.clear();
		}
	}
	return triangleDrawn;
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
