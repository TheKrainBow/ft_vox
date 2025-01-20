#include "World.hpp"
#include "define.hpp"
#include <cmath>
#include <iostream>
#include <memory>
#include <unordered_map>
#include <tuple>

// Helper function to calculate block position within a chunk
vec3 World::calculateBlockPos(int x, int y, int z) const {
	auto mod = [](int value) { return (value >= 0) ? value % CHUNK_SIZE : (CHUNK_SIZE + (value % CHUNK_SIZE)) % CHUNK_SIZE; };
	return { mod(x), mod(y), mod(z) };
}

void World::findOrLoadChunk(vec3 position, std::unordered_map<std::tuple<int, int, int>, std::unique_ptr<Chunk>>& tempChunks)
{
    NoiseGenerator::PerlinMap *perlinMap = nullptr;
    perlinMap = _perlinGenerator.getPerlinMap(position.x, position.z);
    if (!perlinMap)
        return ;
    if (100.0 + perlinMap->heighest * 100.0 < (position.y - 1) * CHUNK_SIZE)
        return ;
    auto currentTuple = std::make_tuple((int)position.x, (int)position.y, (int)position.z);
	if (auto it = _loadedChunks.find(currentTuple); it != _loadedChunks.end())
	{
		tempChunks[currentTuple] = std::move(it->second);
		_loadedChunks.erase(it);
	}
	// else if (auto it = _cachedChunks.find(currentTuple); it != _cachedChunks.end())
	// {
	// 	tempChunks[currentTuple] = std::move(it->second);
	// 	_cachedChunks.erase(it);
	// }
	else
	{
		auto newChunk = std::make_unique<Chunk>(position.x, position.y, position.z, perlinMap, *this);
		tempChunks[currentTuple] = std::move(newChunk);
	}
}

World::World(int seed) : _perlinGenerator(seed) {}

NoiseGenerator &World::getNoiseGenerator(void)
{
    return (_perlinGenerator);
}

World::~World() = default;

void World::loadPerlinMap(vec3 camPosition)
{
    _perlinGenerator.clearPerlinMaps();
	std::unordered_map<std::tuple<int, int, int>, std::unique_ptr<Chunk>> tempChunks;

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

void World::loadChunk(vec3 camPosition)
{
	std::unordered_map<std::tuple<int, int, int>, std::unique_ptr<Chunk>> tempChunks;

	vec3 position;
	for (int x = -XZ_RENDER_DISTANCE; x < XZ_RENDER_DISTANCE; x++)
	{
		for (int y = -Y_RENDER_DISTANCE; y < Y_RENDER_DISTANCE; y++)
		{
			for (int z = -XZ_RENDER_DISTANCE; z < XZ_RENDER_DISTANCE; z++)
			{
				position.x = trunc(camPosition.x / CHUNK_SIZE) + x;
				position.y = trunc(camPosition.y / CHUNK_SIZE) + y;
				position.z = trunc(camPosition.z / CHUNK_SIZE) + z;
				findOrLoadChunk(position, tempChunks);
			}
		}
	}

	//_cachedChunks.insert(std::make_move_iterator(_loadedChunks.begin()), std::make_move_iterator(_loadedChunks.end()));
	_loadedChunks = std::move(tempChunks);
}

char World::getBlock(int x, int y, int z)
{
    vec3 chunkPos(x / CHUNK_SIZE, y / CHUNK_SIZE, z / CHUNK_SIZE);
    chunkPos.x -= (x < 0 && abs(x) % CHUNK_SIZE != 0);
    chunkPos.y -= (y < 0 && abs(y) % CHUNK_SIZE != 0);
    chunkPos.z -= (z < 0 && abs(z) % CHUNK_SIZE != 0);

    Chunk* chunk = getChunk((int)chunkPos.x, (int)chunkPos.y, (int)chunkPos.z);
    if (!chunk) return 'D';

    vec3 blockPos = calculateBlockPos(x, y, z);
    return chunk->getBlock((int)blockPos.x, (int)blockPos.y, (int)blockPos.z);
}

Chunk* World::getChunk(int chunkX, int chunkY, int chunkZ)
{
    auto it = _loadedChunks.find(std::make_tuple(chunkX, chunkY, chunkZ));
    if (it != _loadedChunks.end())
        return it->second.get();
    // auto itt = _cachedChunks.find(std::make_tuple(chunkX, chunkY, chunkZ));
    // if (itt != _cachedChunks.end())
    //     return itt->second.get();
    return nullptr;
}

void World::sendFacesToDisplay()
{
    for (auto& chunk : _loadedChunks)
	{
        if (chunk.second)
            chunk.second->sendFacesToDisplay();
    }
}
