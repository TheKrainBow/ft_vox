#include "Chunk.hpp"

Chunk::Chunk(vec2 position, PerlinMap *perlinMap, World &world, TextureManager &textureManager) : _world(world), _textureManager(textureManager)
{
	_perlinMap = perlinMap;
	std::lock_guard<std::mutex> lock(_subChunksMutex);
	for (int y = (perlinMap->lowest - CHUNK_SIZE) ; y <= (perlinMap->heighest + (CHUNK_SIZE)); y += CHUNK_SIZE)
	{
		_subChunks.push_back(new SubChunk(vec3(position.x, y / CHUNK_SIZE, position.y), perlinMap, world, textureManager));
		// findOrLoadChunk(position, tempChunks, textManager, _perlinMap);
	}
}

Chunk::~Chunk()
{
	for (auto it = _subChunks.begin() ; it != _subChunks.end() ; it++)
		delete *it;
	_subChunks.clear();
}


int Chunk::display()
{
	std::lock_guard<std::mutex> lock(_subChunksMutex);
	int triangleDrawn = 0;
	for (auto it = _subChunks.begin() ; it != _subChunks.end() ; it++)
		triangleDrawn += (*it)->display();
	return triangleDrawn;
}

SubChunk *Chunk::getSubChunk(int y)
{
	if (_isInit == false)
		return nullptr;
	for (auto it = _subChunks.begin() ; it != _subChunks.end() ; it++)
	{
		if ((*it)->getPosition().y == y)
			return (*it);
	}
	return nullptr;
}