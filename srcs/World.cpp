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

World::World(int seed, TextureManager &textureManager, Camera &camera) : _perlinGenerator(seed), _textureManager(textureManager), _camera(&camera), _threadPool(6)
{
	_renderDistance = RENDER_DISTANCE;

	_displayedChunk = new ChunkSlot[_renderDistance * _renderDistance];
	for (int i = 0; i < _renderDistance * _renderDistance; ++i)
		_displayedChunk[i].chunk = nullptr;

	generateSpiralOrder();
}

World::~World()
{
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
	//TODO Save or do not unload modified chunks (delete block)
	//(Add a isModified boolean in Chunk or SubChunk class)
	//TODO Protect from display segv when cache_size is smaller than double the surface
	// (defines to change to reproduce: CACHE_SIZE 2500 RENDER_DISTANCE 61)
	_chunksListMutex.lock();
	if (_chunkList.size() < CACHE_SIZE)
	{
		_chunksListMutex.unlock();
		return;
	}

	// Get player position in chunk coordinates
	vec3 playerPos = _camera->getWorldPosition();
	int playerChunkX = playerPos.x / CHUNK_SIZE;
	int playerChunkZ = playerPos.z / CHUNK_SIZE;
	if (playerPos.x < 0) playerChunkX--;
	if (playerPos.z < 0) playerChunkZ--;

	// Find the farthest chunk
	auto farthestChunkIt = _chunkList.end();
	float maxDistance = -1.0f;
	for (auto it = _chunkList.begin(); it != _chunkList.end(); ++it)
	{
		Chunk* chunk = *it;
		int chunkX = chunk->getPosition().x;
		int chunkZ = chunk->getPosition().y;
		float distance = sqrt(pow(chunkX - playerChunkX, 2) + pow(chunkZ - playerChunkZ, 2));
		if (distance > maxDistance)
		{
			maxDistance = distance;
			farthestChunkIt = it;
		}
	}

	if (farthestChunkIt != _chunkList.end())
	{
		Chunk* chunkToRemove = *farthestChunkIt;
		// Remove from _chunkList
		_chunkList.erase(farthestChunkIt);
		_chunksListMutex.unlock();

		std::pair<int, int> key(chunkToRemove->getPosition().x, chunkToRemove->getPosition().y);

		// Remove from _chunks
		_chunksMutex.lock();
		_chunks.erase(key);
		_chunksMutex.unlock();

		_perlinGenerator.removePerlinMap(key.first, key.second);
		// Clean up memory
		delete chunkToRemove;
	}
	else
	{
		_chunksListMutex.unlock();
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
		chunk = new Chunk({pair.first, pair.second}, _perlinGenerator.getPerlinMap(pair.first, pair.second), *this, _textureManager);

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
	unloadChunk();
}

void World::loadTopChunks(int renderDistance, int render, vec3 camPosition)
{
	vec3 oldCamChunk = vec3(camPosition.x / CHUNK_SIZE, camPosition.y / CHUNK_SIZE, camPosition.z / CHUNK_SIZE);
	if (oldCamChunk.x < 0) oldCamChunk.x--;
	if (oldCamChunk.z < 0) oldCamChunk.z--;

	int z = 0;
	for (int x = 1; x < render && !hasMoved(oldCamChunk); x++)
	{
		loadChunk(x, z, renderDistance, render, camPosition);
	}
}

void World::loadBotChunks(int renderDistance, int render, vec3 camPosition)
{
	vec3 oldCamChunk = vec3(camPosition.x / CHUNK_SIZE, camPosition.y / CHUNK_SIZE, camPosition.z / CHUNK_SIZE);
	if (oldCamChunk.x < 0) oldCamChunk.x--;
	if (oldCamChunk.z < 0) oldCamChunk.z--;

	int z = render - 1;
	for (int x = render - 2; x >= 0 && !hasMoved(oldCamChunk); x--)
	{
		loadChunk(x, z, renderDistance, render, camPosition);
	}
}

void World::loadRightChunks(int renderDistance, int render, vec3 camPosition)
{
	vec3 oldCamChunk = vec3(camPosition.x / CHUNK_SIZE, camPosition.y / CHUNK_SIZE, camPosition.z / CHUNK_SIZE);
	if (oldCamChunk.x < 0) oldCamChunk.x--;
	if (oldCamChunk.z < 0) oldCamChunk.z--;

	int x = render - 1;
	for (int z = 1; z < render && !hasMoved(oldCamChunk); z++)
	{
		loadChunk(x, z, renderDistance, render, camPosition);
	}
}

void World::loadLeftChunks(int renderDistance, int render, vec3 camPosition)
{
	vec3 oldCamChunk = vec3(camPosition.x / CHUNK_SIZE, camPosition.y / CHUNK_SIZE, camPosition.z / CHUNK_SIZE);
	if (oldCamChunk.x < 0) oldCamChunk.x--;
	if (oldCamChunk.z < 0) oldCamChunk.z--;

	int x = 0;
	for (int z = render - 2; z >= 0 && !hasMoved(oldCamChunk); z--)
	{
		loadChunk(x, z, renderDistance, render, camPosition);
	}
}

void World::loadChunks(vec3 camPosition) {
    int renderDistance = _renderDistance;
    vec3 oldCamChunk = vec3(camPosition.x / CHUNK_SIZE, camPosition.y / CHUNK_SIZE, camPosition.z / CHUNK_SIZE);
    if (oldCamChunk.x < 0) oldCamChunk.x--;
    if (oldCamChunk.z < 0) oldCamChunk.z--;
    _skipLoad = false;

	// resetTerrain();
    loadChunk(0, 0, renderDistance, 1, camPosition);
	std::vector<std::future<void>> retLst;
    for (int render = 2; getIsRunning() && render <= renderDistance; render += 2)
	{
		retLst.emplace_back(_threadPool.enqueue(&World::loadTopChunks, this, renderDistance, render, camPosition));
		retLst.emplace_back(_threadPool.enqueue(&World::loadBotChunks, this, renderDistance, render, camPosition));
		retLst.emplace_back(_threadPool.enqueue(&World::loadRightChunks, this, renderDistance, render, camPosition));
		retLst.emplace_back(_threadPool.enqueue(&World::loadLeftChunks, this, renderDistance, render, camPosition));

		if (hasMoved(oldCamChunk))
			break;
    }
	for (std::future<void> &ret : retLst)
	{
		ret.get();
	}
}

bool World::hasMoved(vec3 oldPos)
{
	vec3 newPos = _camera->getWorldPosition();
	vec3 camChunk(newPos.x / CHUNK_SIZE, newPos.y / CHUNK_SIZE, newPos.z / CHUNK_SIZE);
	if (newPos.x < 0) camChunk.x--;
	if (newPos.z < 0) camChunk.z--;

	if (((floor(oldPos.x) != floor(camChunk.x) || floor(oldPos.z) != floor(camChunk.z))))
		return true;
	return false;
}

char World::getBlock(vec3 position)
{
	// std::cout << "World::getBlock" << std::endl;
	vec3 chunkPos((int)position.x / CHUNK_SIZE, (int)position.y / CHUNK_SIZE, (int)position.z / CHUNK_SIZE);
	chunkPos.x -= (position.x < 0 && abs((int)position.x) % CHUNK_SIZE != 0);
	chunkPos.z -= (position.z < 0 && abs((int)position.z) % CHUNK_SIZE != 0);
	chunkPos.y -= (position.y < 0 && abs((int)position.y) % CHUNK_SIZE != 0);
	chunkPos.x = int(chunkPos.x);
	chunkPos.y = int(chunkPos.y);
	chunkPos.z = int(chunkPos.z);

	Chunk *chunk = getChunk(vec2(chunkPos.x, chunkPos.z));
	if (!chunk)
	{
		// std::cout << "No chunk (" << chunkPos.x << ", " << chunkPos.z << ")" << std::endl;
		return 0;
	}
	SubChunk *subChunk = chunk->getSubChunk(chunkPos.y);
	// SubChunk *chunk = getSubChunk(chunkPos);
	if (!subChunk)
	{
		// std::cout << "No subchunk (" << chunkPos.y << ")" << std::endl;
		//std::cout << "Couldn't get Subchunk in: " << chunkPos.x << " " << chunkPos.y << " " << chunkPos.z << std::endl;
		return 0;
	}
	return subChunk->getBlock(calculateBlockPos(position));
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
	auto it = _chunks.find({position.x, position.y});
	auto itend = _chunks.end();
	_chunksMutex.unlock();
	if (it != itend)
		return it->second;
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
		_displayedChunk[x + z * _renderDistance].mutex.lock();
		_displayedChunk[x + z * _renderDistance].chunk = nullptr;
		_displayedChunk[x + z * _renderDistance].mutex.unlock();
	}
}

int World::display()
{
	int triangleDrawn = 0;

	for (auto [x, z] : _spiralOrder)
	{
		Chunk* chunkToDisplay = nullptr;
		_displayedChunk[x + z * _renderDistance].mutex.lock();
		chunkToDisplay = _displayedChunk[x + z * _renderDistance].chunk;
		_displayedChunk[x + z * _renderDistance].mutex.unlock();
		if (chunkToDisplay)
			triangleDrawn += chunkToDisplay->display();
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
