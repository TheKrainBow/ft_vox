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
}

World::~World()
{
	std::lock_guard<std::mutex> lock(_chunksMutex);
	for (auto it = _chunks.begin(); it != _chunks.end(); it++)
		delete it->second;
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

		//_perlinGenerator.removePerlinMap(key.first, key.second);
		// Clean up memory
		delete chunkToRemove;
	}
	else
	{
		_chunksListMutex.unlock();
	}
}

void World::loadChunk(int x, int z, int render, ivec2 chunkPos)
{
	Chunk *chunk = nullptr;
	std::pair<int, int> pair(chunkPos.x - render / 2 + x, chunkPos.y - render / 2 + z);
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

	_chunksLoadMutex.lock();
	_chunksLoadOrder.emplace(chunk);
	_displayedChunks[pair] = chunk;
	_chunksLoadMutex.unlock();
	unloadChunk();
}

void World::loadTopChunks(int render, ivec2 chunkPos)
{
	int z = 0;
	for (int x = 0; x < render; x++)
	{
		loadChunk(x, z, render, chunkPos);
	}
}

void World::loadBotChunks(int render, ivec2 chunkPos)
{
	int z = render - 1;
	for (int x = render - 1; x >= 0; x--)
	{
		loadChunk(x, z, render, chunkPos);
	}
}

void World::loadRightChunks(int render, ivec2 chunkPos)
{
	int x = render - 1;
	for (int z = 0; z < render; z++)
	{
		loadChunk(x, z, render, chunkPos);
	}
}

void World::loadLeftChunks(int render, ivec2 chunkPos)
{
	int x = 0;
	for (int z = render - 1; z >= 0; z--)
	{
		loadChunk(x, z, render, chunkPos);
	}
}

void World::loadFirstChunks(ivec2 chunkPos)
{
    int renderDistance = _renderDistance;
    _skipLoad = false;

    loadChunk(0, 0, 1, chunkPos);
	std::vector<std::future<void>> retLst;
    for (int render = 2; getIsRunning() && render < renderDistance; render += 2)
	{
		retLst.emplace_back(_threadPool.enqueue(&World::loadTopChunks, this, render, chunkPos));
		retLst.emplace_back(_threadPool.enqueue(&World::loadBotChunks, this, render, chunkPos));
		retLst.emplace_back(_threadPool.enqueue(&World::loadRightChunks, this, render, chunkPos));
		retLst.emplace_back(_threadPool.enqueue(&World::loadLeftChunks, this, render, chunkPos));

		if (hasMoved(chunkPos))
			break;
    }
	for (std::future<void> &ret : retLst)
	{
		ret.get();
	}
}

void World::unLoadNextChunks(ivec2 newCamChunk)
{
	std::pair<int, int> pos;
	std::queue<std::pair<int, int>> deleteQueue;
	for (auto &it : _displayedChunks)
	{
		Chunk *chunk = it.second;
		vec2 chunkPos = chunk->getPosition();
		if (abs((int)chunkPos.x - (int)newCamChunk.x) > _renderDistance / 2
		|| abs((int)chunkPos.y - (int)newCamChunk.y) > _renderDistance / 2)
		{
			// std::cout << chunkPos.x << " - " << newCamChunk.x << " = " << abs(chunkPos.x - newCamChunk.x) << " > " << _renderDistance << std::endl;
			// std::cout << chunkPos.y << " - " << newCamChunk.y << " = " << abs(chunkPos.y - newCamChunk.y) << " > " << _renderDistance << std::endl;
			pos = {(int)chunkPos.x, (int)chunkPos.y };
			deleteQueue.emplace(pos);
		}
	}
	// std::cout << deleteQueue.size() << std::endl;
	while (!deleteQueue.empty())
	{
		std::pair<int, int> pos;
		pos = deleteQueue.front();
		_chunksRemovalMutex.lock();
		_chunkRemovalOrder.emplace(pos);
		_chunksRemovalMutex.unlock();
		_displayedChunks.erase(pos);
		deleteQueue.pop();
	}
}

bool World::hasMoved(ivec2 oldPos)
{
	ivec2 camChunk = _camera->getChunkPosition(CHUNK_SIZE);

	if (((floor(oldPos.x) != floor(camChunk.x) || floor(oldPos.y) != floor(camChunk.y))))
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

void World::loadOrder()
{
	_chunksLoadMutex.lock();
	while (!_chunksLoadOrder.empty())
	{
		Chunk *chunk = nullptr;
		chunk = _chunksLoadOrder.front();
		vec2 chunkPos = chunk->getPosition();
		_activeChunks[{chunkPos.x, chunkPos.y}] = chunk;
		_chunksLoadOrder.pop();
	}
	_chunksLoadMutex.unlock();
}

void World::removeOrder()
{
	_chunksRemovalMutex.lock();
	while (!_chunkRemovalOrder.empty())
	{
		std::pair<int, int> pos;
		pos = _chunkRemovalOrder.front();
		_activeChunks.erase(pos);
		_chunkRemovalOrder.pop();
	}
	_chunksRemovalMutex.unlock();
}

int World::display()
{
	int trianglesDrawn = 0;
	removeOrder();
	loadOrder();
	for (auto &activeChunk : _activeChunks)
	{
		trianglesDrawn += activeChunk.second->display();
	}
	return trianglesDrawn;
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
