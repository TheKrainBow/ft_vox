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
	std::unordered_map<std::tuple<int, int, int>, Chunk *> tempChunks;

    (void)renderDistance;
    vec3 position;
    for (int x = -XZ_RENDER_DISTANCE; x < XZ_RENDER_DISTANCE; x++)
    {
        for (int y = -Y_RENDER_DISTANCE; y < Y_RENDER_DISTANCE; y++)
        {
            for (int z = -XZ_RENDER_DISTANCE; z < XZ_RENDER_DISTANCE; z++)
            {
                position.x = trunc(camPosition.x) / CHUNK_SIZE + x;
                position.y = trunc(camPosition.y) / CHUNK_SIZE + y;
                position.z = trunc(camPosition.z) / CHUNK_SIZE + z;
                if (camPosition.x < 0) position.x--;
                if (camPosition.y < 0) position.y--;
                if (camPosition.z < 0) position.z--;
                auto currentTuple = std::make_tuple(trunc(position.x), trunc(position.y), trunc(position.z));
                if (_loadedChunks.find(currentTuple) != _loadedChunks.end()) {
                    tempChunks[currentTuple] = _loadedChunks[currentTuple];
                    _loadedChunks.erase(currentTuple);
                } else if (_cachedChunks.find(currentTuple) != _cachedChunks.end()) {
                    tempChunks[currentTuple] = _cachedChunks[currentTuple];
                    tempChunks[currentTuple]->load();
                    _cachedChunks.erase(currentTuple);
                } else {
                    Chunk *newChunk = new Chunk(trunc(position.x), trunc(position.y), trunc(position.z), *this);
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
    if (x < 0 && abs(x) % 16 != 0) chunkPos.x--;
    if (y < 0 && abs(y) % 16 != 0) chunkPos.y--;
    if (z < 0 && abs(z) % 16 != 0) chunkPos.z--;
    Chunk *chunk = getChunk(chunkPos.x, chunkPos.y, chunkPos.z);
    if (!chunk)
        return 'D';
    vec3 blockPos;
    if (x >= 0)
        blockPos.x = abs(x) % 16;
    else
        blockPos.x = (16 - (abs(x) % 16)) % 16;
    if (y >= 0)
        blockPos.y = abs(y) % 16;
    else
        blockPos.y = (16 - (abs(y) % 16)) % 16;
    if (z >= 0)
        blockPos.z = abs(z) % 16;
    else
        blockPos.z = (16 - (abs(z) % 16)) % 16;
    return chunk->getBlock((int)blockPos.x, (int)blockPos.y, (int)blockPos.z);
}

Chunk* World::getChunk(int chunkX, int chunkY, int chunkZ)
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