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

    std::function<void(vec3)> recursiveLoad = [&](vec3 pos) {
        auto currentTuple = std::make_tuple(
            std::round(pos.x / CHUNK_SIZEV2),
            std::round(pos.y / CHUNK_SIZEV2),
            std::round(pos.z / CHUNK_SIZEV2)
        );

        if (tempChunks.find(currentTuple) != tempChunks.end()) {
            return;
        }

        if (_loadedChunks.find(currentTuple) != _loadedChunks.end()) {
            tempChunks[currentTuple] = _loadedChunks[currentTuple];
            _loadedChunks.erase(currentTuple);
        } else if (_cachedChunks.find(currentTuple) != _cachedChunks.end()) {
            tempChunks[currentTuple] = _cachedChunks[currentTuple];
            tempChunks[currentTuple]->load();
            _cachedChunks.erase(currentTuple);
        } else {
            ChunkV2 *newChunk = new ChunkV2(pos.x / CHUNK_SIZEV2, pos.y / CHUNK_SIZEV2, pos.z / CHUNK_SIZEV2, *this);
            tempChunks[currentTuple] = newChunk;
            // std::cout << "Loading chunk (" << pos.x / CHUNK_SIZEV2 << ", " << pos.y / CHUNK_SIZEV2 << ", " << pos.z / CHUNK_SIZEV2 << ")" << std::endl;
            newChunk->load();
        }

        if (getSquaredDist(position, pos) <= renderDistance * renderDistance) {
            recursiveLoad(vec3(pos.x + CHUNK_SIZEV2, pos.y, pos.z));
            recursiveLoad(vec3(pos.x - CHUNK_SIZEV2, pos.y, pos.z));
            recursiveLoad(vec3(pos.x, pos.y + CHUNK_SIZEV2, pos.z));
            recursiveLoad(vec3(pos.x, pos.y - CHUNK_SIZEV2, pos.z));
            recursiveLoad(vec3(pos.x, pos.y, pos.z + CHUNK_SIZEV2));
            recursiveLoad(vec3(pos.x, pos.y, pos.z - CHUNK_SIZEV2));
        }
    };
    recursiveLoad(position);
    for (auto& chunk : _loadedChunks) {
        _cachedChunks[chunk.first] = chunk.second;
        chunk.second->unload();
    }

    // std::cout << tempChunks.size() << std::endl;
    _loadedChunks = std::move(tempChunks);
    std::cout << _loadedChunks.size() << std::endl;
}

BlockType World::getBlock(int x, int y, int z)
{
    auto chunk = _loadedChunks.find(std::make_tuple(
        static_cast<int>(x / CHUNK_SIZEV2),
        static_cast<int>(y / CHUNK_SIZEV2),
        static_cast<int>(z / CHUNK_SIZEV2)
    ));
    if (chunk == _loadedChunks.end())
        return AIR;
    if (!(*chunk).second)
        return AIR;
    return (*chunk).second->getBlock(abs(x) % 16, abs(y) % 16, abs(z) % 16);
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