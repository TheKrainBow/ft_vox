#include "Chunk.hpp"

Chunk::Chunk(vec2 position, PerlinMap *perlinMap, World &world, TextureManager &textureManager) : _world(world), _textureManager(textureManager)
{
	_perlinMap = perlinMap;
	std::lock_guard<std::mutex> lock(_subChunksMutex);
	for (int y = (perlinMap->lowest) - 1 ; y < (perlinMap->heighest) + CHUNK_SIZE; y += CHUNK_SIZE)
	{
		SubChunk *newChunk = new SubChunk(vec3(position.x, y / CHUNK_SIZE, position.y), perlinMap, world, textureManager);
		_subChunks.push_back(newChunk);
	}
	_isInit = true;
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
	vec3 chunkPos;
	for (auto it = _subChunks.begin() ; it != _subChunks.end() ; it++)
	{
		chunkPos = (*it)->getPosition();
		// std::cout << chunkPos.x << ", " << chunkPos.y << ", " << chunkPos.z << std::endl;
		triangleDrawn += (*it)->display();
	}
	return triangleDrawn;
}

SubChunk *Chunk::getSubChunk(int y)
{
	if (_isInit == false)
		return nullptr;
	vec3 chunkPos;
	for (auto it = _subChunks.begin() ; it != _subChunks.end() ; it++)
	{
		chunkPos = (*it)->getPosition();
		if (chunkPos.y == y)
			return (*it);
	}
	return nullptr;
}

void Chunk::sendFacesToDisplay()
{
	std::lock_guard<std::mutex> lock(_subChunksMutex);
	for (auto subChunk : _subChunks)
		subChunk->sendFacesToDisplay();
}