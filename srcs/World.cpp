#include "World.hpp"


NoiseGenerator &World::getNoiseGenerator(void)
{
    return (_perlinGenerator);
}

World::World(int seed) : _perlinGenerator(seed)
{
    _loadedChunks.clear();
}

World::~World()
{
    for (auto &chunk : _loadedChunks)
        if (chunk.second)
        {
            chunk.second->unload();
            delete chunk.second;
        }
    for (auto &chunk : _cachedChunks)
        if (chunk.second)
        {
            chunk.second->unload();
            delete chunk.second;
        }
}

double getSquaredDist(vec3 a, vec3 b)
{
    vec3 c(a.x - b.x, a.y - b.y, a.z - b.z);
    return (c.x * c.x + c.y * c.y + c.z * c.z);
}
    
void World::loadChunk(vec3 position, int renderDistance)
{
    std::unordered_map<std::tuple<int, int, int>, ChunkV2 *> tempChunks;

    position.x = trunc(position.x) / CHUNK_SIZEV2;
    position.y = trunc(position.y) / CHUNK_SIZEV2;
    position.z = trunc(position.z) / CHUNK_SIZEV2;
    for (int x = 0; x < renderDistance; x++)
    {
        for (int y = 0; y < renderDistance; y++)
        {
            for (int z = 0; z < renderDistance; z++)
            {
                auto currentTuple = std::make_tuple(trunc(position.x) - renderDistance / 2 + x, trunc(position.y) - renderDistance / 2 + y, trunc(position.z) - renderDistance / 2 + z);
                if (_loadedChunks.find(currentTuple) != _loadedChunks.end()) {
                    tempChunks[currentTuple] = _loadedChunks[currentTuple];
                    _loadedChunks.erase(currentTuple);
                } else if (_cachedChunks.find(currentTuple) != _cachedChunks.end()) {
                    tempChunks[currentTuple] = _cachedChunks[currentTuple];
                    tempChunks[currentTuple]->load();
                    _cachedChunks.erase(currentTuple);
                } else {
                    ChunkV2 *newChunk = new ChunkV2(trunc(position.x) - renderDistance / 2 + x, trunc(position.y) - renderDistance / 2 + y, trunc(position.z) - renderDistance / 2 + z, *this);
                    tempChunks[currentTuple] = newChunk;
                    newChunk->load();
                    // std::cout << "Loading chunk (" << pos.x / CHUNK_SIZEV2 << ", " << pos.y / CHUNK_SIZEV2 << ", " << pos.z / CHUNK_SIZEV2 << ")" << std::endl;
                }
            }
        }
    }

    for (auto& chunk : _loadedChunks) {
        _cachedChunks[chunk.first] = chunk.second;
        chunk.second->unload();
    }

    _loadedChunks = std::move(tempChunks);
    for (auto& chunk: _loadedChunks)
        chunk.second->updateNeighbors();
    std::cout << _loadedChunks.size() << std::endl;
}

BlockType World::getBlock(int x, int y, int z)
{
    auto chunk = _loadedChunks.find(std::make_tuple(
        static_cast<int>(x) / CHUNK_SIZEV2,
        static_cast<int>(y) / CHUNK_SIZEV2,
        static_cast<int>(z) / CHUNK_SIZEV2
    ));
    if (chunk == _loadedChunks.end())
    {
        return AIR;
    }
    if (!(*chunk).second)
    {
        return AIR;
    }
    return (*chunk).second->getBlock(static_cast<int>(x) / CHUNK_SIZEV2, static_cast<int>(y) / CHUNK_SIZEV2, static_cast<int>(z) / CHUNK_SIZEV2);
}

void World::sendFacesToDisplay()
{
    for (auto& chunk : _loadedChunks) {
        if (chunk.second)
            chunk.second->sendFacesToDisplay();
        else
            std::cout << "chunk.second is null: " << std::endl;
    }
}