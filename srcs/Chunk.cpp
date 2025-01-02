#include "Chunk.hpp"

Chunk::Chunk(int chunkX, int chunkZ)
{
	_position = vec2(chunkX, chunkZ);
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

void Chunk::display(void)
{
	for (int x = 0; x < CHUNK_SIZE_X; ++x) {
		for (int z = 0; z < CHUNK_SIZE_Z; ++z) {
			for (int y = 0; y < 6; ++y) { // Only iterate Y < 10
				int index = x + z * CHUNK_SIZE_X + y * CHUNK_SIZE_X * CHUNK_SIZE_Z;
				_blocks[index]->display();
			}
		}
	}
}

Chunk::~Chunk()
{
	//for (int i = 0; i < CHUNK_SIZE_X * CHUNK_SIZE_Z * CHUNK_SIZE_Y; i++)
	//	delete _blocks[i];
}
