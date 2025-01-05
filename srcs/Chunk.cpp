#include "Chunk.hpp"
#include "NoiseGenerator.hpp"

Chunk::Chunk(int chunkX, int chunkZ, NoiseGenerator &noise_gen)
{
	_position = vec2(chunkX, chunkZ);
	bzero(_blocks, CHUNK_SIZE_X * CHUNK_SIZE_Z * CHUNK_SIZE_Y * sizeof(ABlock *));

	// Generate terrain. (Must be refactored using Perlin Noise)
	for (int x = 0; x < 16; x++)
	{
		for (int z = 0; z < 16; z++)
		{
			double scaledX = (chunkX * 16.0 + x);
			double scaledZ = (chunkZ * 16.0 + z);
			double noise = noise_gen.noise(scaledX, scaledZ);
			double remappedNoise = (int)(((noise + 1.0) * 0.5) * 25.0);
			size_t maxHeight = (size_t)(remappedNoise);
			for (size_t y = 0; y < maxHeight / 2; y++)
				_blocks[x + (z * CHUNK_SIZE_X) + (y * CHUNK_SIZE_X * CHUNK_SIZE_Z)] = new Stone((chunkX * CHUNK_SIZE_X) + x, y, (chunkZ * CHUNK_SIZE_Z) + z);
			for (size_t y = maxHeight / 2; y < maxHeight; y++)
				_blocks[x + (z * CHUNK_SIZE_X) + (y * CHUNK_SIZE_X * CHUNK_SIZE_Z)] = new Dirt((chunkX * CHUNK_SIZE_X) + x, y, (chunkZ * CHUNK_SIZE_Z) + z);
			_blocks[x + (z * CHUNK_SIZE_X) + (maxHeight * CHUNK_SIZE_X * CHUNK_SIZE_Z)] = new Grass((chunkX * CHUNK_SIZE_X) + x, maxHeight, (chunkZ * CHUNK_SIZE_Z) + z);
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
