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
            delete chunk.second;
    for (auto &chunk : _cachedChunks)
        if (chunk.second)
            delete chunk.second;
}

double getSquaredDist(vec3 a, vec3 b)
{
    vec3 c(a.x - b.x, a.y - b.y, a.z - b.z);
    return (c.x * c.x + c.y * c.y + c.z * c.z);
}
    
void World::loadChunk(vec3 camPosition, int renderDistance)
{
	std::unordered_map<std::tuple<int, int, int>, ChunkV2 *> tempChunks;

    (void)renderDistance;
    vec3 position;
    position.x = trunc(camPosition.x) / CHUNK_SIZEV2;
    position.y = trunc(camPosition.y) / CHUNK_SIZEV2;
    position.z = trunc(camPosition.z) / CHUNK_SIZEV2;
    if (camPosition.x < 0) position.x--;
    if (camPosition.y < 0) position.y--;
    if (camPosition.z < 0) position.z--;
    for (int x = -XZ_RENDER_DISTANCE; x < XZ_RENDER_DISTANCE; x++)
    {
        for (int y = -Y_RENDER_DISTANCE; y < Y_RENDER_DISTANCE; y++)
        {
            for (int z = -XZ_RENDER_DISTANCE; z < XZ_RENDER_DISTANCE; z++)
            {
                auto currentTuple = std::make_tuple(trunc(position.x) + x, trunc(position.y) + y, trunc(position.z) + z);
                if (_loadedChunks.find(currentTuple) != _loadedChunks.end()) {
                    tempChunks[currentTuple] = _loadedChunks[currentTuple];
                    _loadedChunks.erase(currentTuple);
                } else if (_cachedChunks.find(currentTuple) != _cachedChunks.end()) {
                    tempChunks[currentTuple] = _cachedChunks[currentTuple];
                    tempChunks[currentTuple]->load();
                    _cachedChunks.erase(currentTuple);
                } else {
                    ChunkV2 *newChunk = new ChunkV2(trunc(position.x) + x, trunc(position.y) + y, trunc(position.z) + z, *this);
                    tempChunks[currentTuple] = newChunk;
                    newChunk->load();
                }
            }
        }
    }

    for (auto& chunk : _loadedChunks) {
        _cachedChunks[chunk.first] = chunk.second;
    }
    _loadedChunks = std::move(tempChunks);
}

char World::getBlock(int x, int y, int z)
{
    vec3 chunkPos(x / 16, y / 16, z / 16);
    // if (x < 0) chunkPos.x--;
    // if (y < 0) chunkPos.y--;
    // if (z < 0) chunkPos.z--;
    ChunkV2 *chunk = getChunk(chunkPos.x, chunkPos.y, chunkPos.z);
    if (!chunk)
        return 'D';
    return chunk->getBlock(abs(x) % 16, abs(y) % 16, abs(z) % 16);
}

ChunkV2* World::getChunk(int chunkX, int chunkY, int chunkZ)
{
	auto it = _loadedChunks.find(std::make_tuple(chunkX, chunkY, chunkZ));
	if (it != _loadedChunks.end())
		return it->second;
	auto itt = _cachedChunks.find(std::make_tuple(chunkX, chunkY, chunkZ));
	if (itt != _cachedChunks.end())
		return itt->second;
	return nullptr;
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