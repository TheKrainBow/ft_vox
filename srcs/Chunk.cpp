#include "Chunk.hpp"

Chunk::Chunk(vec2 position, PerlinMap *perlinMap, World &world, TextureManager &textureManager) : _world(world), _textureManager(textureManager)
{
	_perlinMap = perlinMap;
	for (int y = (perlinMap->lowest - CHUNK_SIZE) ; y <= (perlinMap->heighest + (CHUNK_SIZE)); y += CHUNK_SIZE)
	{
		_subChunks.push_back(SubChunk(vec3(position.x, y / CHUNK_SIZE, position.y), perlinMap, world, textureManager));
		// findOrLoadChunk(position, tempChunks, textManager, _perlinMap);
	}
}

Chunk::~Chunk()
{
	_subChunks.clear();
}
