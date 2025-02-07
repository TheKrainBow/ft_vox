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

World::World(int seed, TextureManager &textureManager) : _perlinGenerator(seed), _textureManager(textureManager) {
	_displayedChunk = new Chunk*[_maxRender * _maxRender];
	bzero(_displayedChunk, _maxRender * _maxRender);
	_renderDistance = RENDER_DISTANCE;
}

World::~World() {
	for (auto it = _chunks.begin(); it != _chunks.end(); it++)
		delete it->second;
	delete [] _displayedChunk;
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


void World::loadChunk(int x, int z, int renderMax, int currentRender, vec3 camPosition)
{
	Chunk *chunk;
	int correctX = (renderMax / 2) - (currentRender / 2) + x;
	int correctZ = (renderMax / 2) - (currentRender / 2) + z;
	std::pair<int, int> pair(camPosition.x / CHUNK_SIZE - currentRender / 2 + x, camPosition.z / CHUNK_SIZE - currentRender / 2 + z);
	auto it = _chunks.find(pair);
	if (it != _chunks.end())
		chunk = it->second;
	else
	{
		chunk = new Chunk(vec2(pair.first, pair.second), _perlinGenerator.getPerlinMap(pair.first, pair.second), *this, _textureManager);
		_displayMutex.lock();
		_chunks[pair] = chunk;
		_displayMutex.unlock();
	}
	_displayMutex.lock();
	_displayedChunk[correctX + correctZ * renderMax] = chunk;
	_displayMutex.unlock();
}

void World::loadChunks(vec3 camPosition)
{
	int renderDistance = _renderDistance;
	for (int render = 1; render < renderDistance; render += 2)
	{
		for (int x = 0; x < render; x++)
		{
			int z = 0;
			loadChunk(x, z, renderDistance, render, camPosition);
		}
		for (int x = 0; x < render; x++)
		{
			int z = render - 1;
			loadChunk(x, z, renderDistance, render, camPosition);
		}
		for (int z = 1; z < render - 1; z++)
		{
			int x = 0;
			loadChunk(x, z, renderDistance, render, camPosition);
		}
		for (int z = 1; z < render - 1; z++)
		{
			int x = render - 1;
			loadChunk(x, z, renderDistance, render, camPosition);
		}
	}
}

char World::getBlock(vec3 position)
{
	vec3 chunkPos(position);
	chunkPos /= CHUNK_SIZE;
	chunkPos.x -= (position.x < 0 && abs((int)position.x) % CHUNK_SIZE != 0);
	chunkPos.y -= (position.y < 0 && abs((int)position.y) % CHUNK_SIZE != 0);
	chunkPos.z -= (position.z < 0 && abs((int)position.z) % CHUNK_SIZE != 0);

	SubChunk* chunk = getChunk(chunkPos);
	if (!chunk)
	{
		// std::cout << "didn't found chunk" << std::endl;
		return 0;
	}
	return chunk->getBlock(calculateBlockPos(position));
}

SubChunk* World::getChunk(vec3 position)
{
	auto it = _chunks.find(std::make_pair(position.x, position.z));
	if (it != _chunks.end())
		return it->second->getSubChunk(position.y);
	return nullptr;
}

int World::display()
{
	int triangleDrawn = 0;
	for (int x = 0; x < _renderDistance; x++)
	{
		for (int z = 0; z < _renderDistance; z++)
		{
			_displayMutex.lock();
			if (_displayedChunk[x + z * _renderDistance])
				triangleDrawn += _displayedChunk[x + z * _renderDistance]->display();
			_displayMutex.unlock();
		}
	}
	return (triangleDrawn);
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