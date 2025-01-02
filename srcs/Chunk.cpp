#include "Chunk.hpp"

Chunk::Chunk(int chunkX, int chunkZ)
{
	_position = vec2(chunkX, chunkZ);
	bzero(_blocks, CHUNK_SIZE_X * CHUNK_SIZE_Z * CHUNK_SIZE_Y * sizeof(ABlock *));
	for (int x = 0; x < 16; x++)
	{
		for (int y = 0; y < 255; y++)
		{
			for (int z = 0; z < 16; z++)
			{
				int state = 0;
				state |= ((y != 5) << 0);
				state |= ((y != 0) << 1);
				state |= ((x != 15) << 2);
				state |= ((x != 0) << 3);
				state |= ((z != 0) << 4);
				state |= ((z != 15) << 5);
				if (y < 3)
					_blocks[x + (z * CHUNK_SIZE_X) + (y * CHUNK_SIZE_X * CHUNK_SIZE_Z)] = new Cobble((chunkX * CHUNK_SIZE_X) + x, y, (chunkZ * CHUNK_SIZE_Z) + z, state);
				else if (y < 6)
					_blocks[x + (z * CHUNK_SIZE_X) + (y * CHUNK_SIZE_X * CHUNK_SIZE_Z)] = new Dirt((chunkX * CHUNK_SIZE_X) + x, y, (chunkZ * CHUNK_SIZE_Z) + z, state);
				else
					_blocks[x + (z * CHUNK_SIZE_X) + (y * CHUNK_SIZE_X * CHUNK_SIZE_Z)] = new Air((chunkX * CHUNK_SIZE_X) + x, y, (chunkZ * CHUNK_SIZE_Z) + z, state);
			}
		}
	}
}

void Chunk::freeChunkData()
{
	for (int i = 0; i < CHUNK_SIZE_X * CHUNK_SIZE_Z * CHUNK_SIZE_Y; i++)
	{
		if (_blocks[i])
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

void Chunk::display(void)
{
	GLuint currentText = -1;
	for (int x = 0; x < CHUNK_SIZE_X; ++x) {
		for (int z = 0; z < CHUNK_SIZE_Z; ++z) {
			for (int y = 0; y < CHUNK_SIZE_Z; ++y) {
				int index = x + z * CHUNK_SIZE_X + y * CHUNK_SIZE_X * CHUNK_SIZE_Z;
				if (_blocks[index]->getType() != DIRT)
					continue ;
				currentText = _blocks[index]->display(currentText);
			}
		}
	}
	for (int x = 0; x < CHUNK_SIZE_X; ++x) {
		for (int z = 0; z < CHUNK_SIZE_Z; ++z) {
			for (int y = 0; y < CHUNK_SIZE_Z; ++y) {
				int index = x + z * CHUNK_SIZE_X + y * CHUNK_SIZE_X * CHUNK_SIZE_Z;
				if (_blocks[index]->getType() != COBBLE)
					continue ;
				currentText = _blocks[index]->display(currentText);
			}
		}
	}
}

Chunk::~Chunk()
{
	for (int i = 0; i < CHUNK_SIZE_X * CHUNK_SIZE_Z * CHUNK_SIZE_Y; i++)
	{
		if (_blocks[i])
			delete _blocks[i];
		_blocks[i] = nullptr;
	}
}
