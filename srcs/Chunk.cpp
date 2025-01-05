#include "Chunk.hpp"

Chunk::Chunk(int chunkX, int chunkZ)
{
	_position = vec2(chunkX, chunkZ);
	bzero(_blocks, CHUNK_SIZE_X * CHUNK_SIZE_Z * CHUNK_SIZE_Y * sizeof(ABlock *));
	// Generate terrain. (Must be refactored using Perlin Noise)
	for (int x = 0; x < 16; x++)
	{
		for (int z = 0; z < 16; z++)
		{
			int maxY = rand() % 100;
			maxY = (maxY > 90) + (maxY > 95) + 1;
			for (int y = 0; y < 10 + maxY / 2; y++)
				_blocks[x + (z * CHUNK_SIZE_X) + (y * CHUNK_SIZE_X * CHUNK_SIZE_Z)] = new Cobble((chunkX * CHUNK_SIZE_X) + x, y, (chunkZ * CHUNK_SIZE_Z) + z);
			for (int y = 10 + maxY / 2; y < 10 + maxY; y++)
				_blocks[x + (z * CHUNK_SIZE_X) + (y * CHUNK_SIZE_X * CHUNK_SIZE_Z)] = new Dirt((chunkX * CHUNK_SIZE_X) + x, y, (chunkZ * CHUNK_SIZE_Z) + z);
			_blocks[x + (z * CHUNK_SIZE_X) + ((10 + maxY) * CHUNK_SIZE_X * CHUNK_SIZE_Z)] = new Grass((chunkX * CHUNK_SIZE_X) + x, 10 + maxY, (chunkZ * CHUNK_SIZE_Z) + z);
		}
	}
	// Check for faces to display (Only for current chunk)
	for (int x = 0; x < CHUNK_SIZE_X; ++x) {
		for (int z = 0; z < CHUNK_SIZE_Z; ++z) {
			for (int y = 0; y < CHUNK_SIZE_Y; ++y) {
				int index = x + z * CHUNK_SIZE_X + y * CHUNK_SIZE_X * CHUNK_SIZE_Z;
				if (!_blocks[index])
						continue ;
				int state = 0;
				bool displayDown = (y == 0) || _blocks[x + z * CHUNK_SIZE_X + (y - 1) * CHUNK_SIZE_X * CHUNK_SIZE_Z] == NULL;
				bool displayUp = (y == 255) || _blocks[x + z * CHUNK_SIZE_X + (y + 1) * CHUNK_SIZE_X * CHUNK_SIZE_Z] == NULL;
				bool displayRight = (x == 0) || _blocks[(x - 1) + z * CHUNK_SIZE_X + y * CHUNK_SIZE_X * CHUNK_SIZE_Z] == NULL;
				bool displayLeft = (x == 15) || _blocks[(x + 1) + z * CHUNK_SIZE_X + y * CHUNK_SIZE_X * CHUNK_SIZE_Z] == NULL;
				bool displayFront = (z == 0) || _blocks[x + (z - 1) * CHUNK_SIZE_X + y * CHUNK_SIZE_X * CHUNK_SIZE_Z] == NULL;
				bool displayBack = (z == 15) || _blocks[x + (z + 1) * CHUNK_SIZE_X + y * CHUNK_SIZE_X * CHUNK_SIZE_Z] == NULL;
				state |= (displayUp << 0);
				state |= (displayDown << 1);
				state |= (displayLeft << 2);
				state |= (displayRight << 3);
				state |= (displayFront << 4);
				state |= (displayBack << 5);
				_blocks[index]->updateNeighbors(state);
			}
		}
	}
}

void Chunk::freeChunkData()
{
	for (int i = 0; i < CHUNK_SIZE_X * CHUNK_SIZE_Z * CHUNK_SIZE_Y; i++)
	{
		if (!_blocks[i])
			continue ;
		delete _blocks[i];
		_blocks[i] = nullptr;
	}
}

Chunk::Chunk(const Chunk& other)
{
	_position = other._position;
	for (int i = 0; i < CHUNK_SIZE_X * CHUNK_SIZE_Z * CHUNK_SIZE_Y; ++i)
	{
		if (other._blocks[i])
			_blocks[i] = other._blocks[i]->clone();
		else
			_blocks[i] = nullptr;
	}
}

void Chunk::display()
{
	for (int i = 0; i < CHUNK_SIZE_X * CHUNK_SIZE_Z * CHUNK_SIZE_Y; ++i)
	{
		if (_blocks[i])
			_blocks[i]->display();
	}
}

Chunk::~Chunk()
{
}

vec2 Chunk::getPosition(void)
{
	return _position;
}