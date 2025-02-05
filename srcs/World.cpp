#include "World.hpp"
#include "define.hpp"
#include <cmath>
#include <iostream>
#include <memory>
#include <map>
#include <tuple>

// Helper function to calculate block position within a chunk
vec3 World::calculateBlockPos(vec3 position) const {
	auto mod = [](int value) { return (value >= 0) ? value % CHUNK_SIZE : (CHUNK_SIZE + (value % CHUNK_SIZE)) % CHUNK_SIZE; };
	return { mod(position.x), mod(position.y), mod(position.z) };
}

void World::findOrLoadChunk(vec3 position, std::map<std::tuple<int, int, int>, std::unique_ptr<SubChunk>>& tempChunks, TextureManager &textManager, PerlinMap *perlinMap)
{
    auto currentTuple = std::make_tuple((int)position.x, (int)position.y, (int)position.z);
	auto it = _loadedChunks.find(currentTuple);
	if (it != _loadedChunks.end())
	{
		tempChunks[currentTuple] = std::move(it->second);
		_loadedChunks.erase(it);
	}
	else if (auto it = _cachedChunks.find(currentTuple); it != _cachedChunks.end())
	{
		tempChunks[currentTuple] = std::move(it->second);
		_cachedChunks.erase(it);
	}
	else
	{
		auto newChunk = std::make_unique<SubChunk>(position, perlinMap, *this, textManager);
		tempChunks[currentTuple] = std::move(newChunk);
	}
}

World::World(int seed) : _perlinGenerator(seed) {
	_chunks = new Chunk*[_maxRender * _maxRender];
}

World::~World() {
	delete [] _chunks;
};

NoiseGenerator &World::getNoiseGenerator(void)
{
	return (_perlinGenerator);
}


void World::loadPerlinMap(vec3 camPosition)
{
	_perlinGenerator.clearPerlinMaps();
	vec2 position;
	for (int x = -XZ_RENDER_DISTANCE; x < XZ_RENDER_DISTANCE; x++)
	{
		for (int z = -XZ_RENDER_DISTANCE; z < XZ_RENDER_DISTANCE; z++)
		{
			position.x = trunc(camPosition.x / CHUNK_SIZE) + x;
			position.y = trunc(camPosition.z / CHUNK_SIZE) + z;
			_perlinGenerator.addPerlinMap(position.x, position.y, CHUNK_SIZE, 1);
		}
	}
}

void World::loadChunk(vec3 camPosition, TextureManager &textManager)
{
	std::map<std::tuple<int, int, int>, std::unique_ptr<SubChunk>> tempChunks;

	vec3 position;
	for (int x = -XZ_RENDER_DISTANCE; x < XZ_RENDER_DISTANCE; x++)
	{
		for (int z = -XZ_RENDER_DISTANCE; z < XZ_RENDER_DISTANCE; z++)
		{
			position.x = trunc(camPosition.x / CHUNK_SIZE) + x;
			position.z = trunc(camPosition.z / CHUNK_SIZE) + z;
			PerlinMap *perlinMap = nullptr;
			perlinMap = _perlinGenerator.getPerlinMap(position.x, position.z);
			for (int y = (perlinMap->lowest - CHUNK_SIZE) ; y <= (perlinMap->heighest + (CHUNK_SIZE)); y += CHUNK_SIZE)
			{
				position.y = y / CHUNK_SIZE;
				findOrLoadChunk(position, tempChunks, textManager, perlinMap);
			}
		}
	}

	_cachedChunks.insert(std::make_move_iterator(_loadedChunks.begin()), std::make_move_iterator(_loadedChunks.end()));
	_loadedChunks = std::move(tempChunks);
}

char World::getBlock(vec3 position)
{
    vec3 chunkPos(position);
	chunkPos /= CHUNK_SIZE;
    chunkPos.x -= (position.x < 0 && abs((int)position.x) % CHUNK_SIZE != 0);
    chunkPos.y -= (position.y < 0 && abs((int)position.y) % CHUNK_SIZE != 0);
    chunkPos.z -= (position.z < 0 && abs((int)position.z) % CHUNK_SIZE != 0);

    SubChunk* chunk = getChunk(chunkPos);
    if (!chunk) return 0;

    return chunk->getBlock(calculateBlockPos(position));
}

SubChunk* World::getChunk(vec3 position)
{
    auto it = _loadedChunks.find(std::make_tuple(position.x, position.y, position.z));
    if (it != _loadedChunks.end())
        return it->second.get();
     auto itt = _cachedChunks.find(std::make_tuple(position.x, position.y, position.z));
     if (itt != _cachedChunks.end())
         return itt->second.get();
    return nullptr;
}

int World::display(Camera &cam, GLFWwindow* win)
{
	(void)cam;
	(void)win;
	int triangleDrown = 0;
	for (auto &chunk : _loadedChunks)
	{
		triangleDrown += chunk.second->display();
	}
	return (triangleDrown);
}


int	World::getLoadedChunksNumber()
{
	return _loadedChunks.size();
}

int	World::getCachedChunksNumber()
{
	return _cachedChunks.size();
}
