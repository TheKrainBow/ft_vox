#include "ChunkV2.hpp"
#include "World.hpp"

ChunkV2::ChunkV2(int x, int y, int z, World &world) : _world(world)
{
	_position = vec3(x, y, z);
	double *perlinMap = _world.getNoiseGenerator().noiseMap(x * CHUNK_SIZEV2, z * CHUNK_SIZEV2, CHUNK_SIZEV2);
	memcpy(_perlinMap, perlinMap, (sizeof(double) * CHUNK_SIZEV2 * CHUNK_SIZEV2));
	delete []perlinMap;
	bzero(_blocks, CHUNK_SIZEV2 * CHUNK_SIZEV2 * CHUNK_SIZEV2 * sizeof(ABlock *));
}

void ChunkV2::load()
{
	for (int y = 0; y < 16 ; y++)
	{
		for (int x = 0; x < 16 ; x++)
		{
			for (int z = 0; z < 16 ; z++)
			{
				double noise = _perlinMap[z * CHUNK_SIZEV2 + x];
				double remappedNoise = (int)(((noise + 1.0) * 0.5) * 25.0);
				size_t maxHeight = (size_t)(remappedNoise);
				if (y + _position.y * CHUNK_SIZEV2 < maxHeight / 2)
					_blocks[x + (z * CHUNK_SIZEV2) + (y * CHUNK_SIZEV2 * CHUNK_SIZEV2)] = new Stone((_position.x * CHUNK_SIZEV2) + x, (_position.y * CHUNK_SIZEV2) + y, (_position.z * CHUNK_SIZEV2) + z);
				else if (y + _position.y * CHUNK_SIZEV2 < maxHeight)
					_blocks[x + (z * CHUNK_SIZEV2) + (y * CHUNK_SIZEV2 * CHUNK_SIZEV2)] = new Dirt((_position.x * CHUNK_SIZEV2) + x, (_position.y * CHUNK_SIZEV2) + y, (_position.z * CHUNK_SIZEV2) + z);
				else if (y + _position.y * CHUNK_SIZEV2 == maxHeight)
					_blocks[x + (z * CHUNK_SIZEV2) + (y * CHUNK_SIZEV2 * CHUNK_SIZEV2)] = new Grass((_position.x * CHUNK_SIZEV2) + x, (_position.y * CHUNK_SIZEV2) + y, (_position.z * CHUNK_SIZEV2) + z);
			}
		}
	}
	_isLoaded = true;
}

void ChunkV2::updateNeighbors()
{
	for (int x = 0; x < CHUNK_SIZEV2; ++x) {
		for (int z = 0; z < CHUNK_SIZEV2; ++z) {
			for (int y = 0; y < CHUNK_SIZEV2; ++y) {
				int index = x + z * CHUNK_SIZEV2 + y * CHUNK_SIZEV2 * CHUNK_SIZEV2;
				if (!_blocks[index])
						continue ;
				int state = 0;
				bool down, up, right, left, front, back;
				if (y == 15)
					up = _world.getBlock(_position.x * CHUNK_SIZEV2 + x, _position.y * CHUNK_SIZEV2 + y + 1, _position.z * CHUNK_SIZEV2 + z) == AIR;
				else
					up = _blocks[x + (z * CHUNK_SIZEV2) + ((y + 1) * CHUNK_SIZEV2 * CHUNK_SIZEV2)] == NULL;
				if (y == 0)
					down = _world.getBlock(_position.x * CHUNK_SIZEV2 + x, _position.y * CHUNK_SIZEV2 + y - 1, _position.z * CHUNK_SIZEV2 + z) == AIR;
				else
					down = _blocks[x + (z * CHUNK_SIZEV2) + ((y - 1) * CHUNK_SIZEV2 * CHUNK_SIZEV2)] == NULL;
				if (x == 0)
					right = _world.getBlock(_position.x * CHUNK_SIZEV2 + x - 1, _position.y * CHUNK_SIZEV2 + y, _position.z * CHUNK_SIZEV2 + z) == AIR;
				else
					right = _blocks[(x - 1) + (z * CHUNK_SIZEV2) + (y * CHUNK_SIZEV2 * CHUNK_SIZEV2)] == NULL;
				if (x == 15)
					left = _world.getBlock(_position.x * CHUNK_SIZEV2 + x + 1, _position.y * CHUNK_SIZEV2 + y, _position.z * CHUNK_SIZEV2 + z) == AIR;
				else
					left = _blocks[(x + 1) + (z * CHUNK_SIZEV2) + (y * CHUNK_SIZEV2 * CHUNK_SIZEV2)] == NULL;
				if (z == 0)
					front = _world.getBlock(_position.x * CHUNK_SIZEV2 + x, _position.y * CHUNK_SIZEV2 + y, _position.z * CHUNK_SIZEV2 + z - 1) == AIR;
				else
					front = _blocks[x + ((z - 1) * CHUNK_SIZEV2) + (y * CHUNK_SIZEV2 * CHUNK_SIZEV2)] == NULL;
				if (z == 15)
					back = _world.getBlock(_position.x * CHUNK_SIZEV2 + x, _position.y * CHUNK_SIZEV2 + y, _position.z * CHUNK_SIZEV2 + z + 1) == AIR;
				else
					back = _blocks[x + ((z + 1) * CHUNK_SIZEV2) + (y * CHUNK_SIZEV2 * CHUNK_SIZEV2)] == NULL;
				state |= (up << 0);
				state |= (down << 1);
				state |= (left << 2);
				state |= (right << 3);
				state |= (front << 4);
				state |= (back << 5);
				_blocks[index]->updateNeighbors(state);
			}
		}
	}
}

void ChunkV2::unload()
{
	for (int i = 0; i < CHUNK_SIZEV2 * CHUNK_SIZEV2 * CHUNK_SIZEV2; i++)
		delete _blocks[i];
}

ChunkV2::~ChunkV2()
{
}

BlockType ChunkV2::getBlock(int x, int y, int z)
{
	if (_isLoaded == false)
		return AIR;
	if (x >= CHUNK_SIZEV2 || y >= CHUNK_SIZEV2 || z >= CHUNK_SIZEV2)
		return AIR;
	x = abs(x);
	y = abs(y);
	z = abs(z);
	ABlock *block = _blocks[x + (z * CHUNK_SIZEV2) + (y * CHUNK_SIZEV2 * CHUNK_SIZEV2)];
	if (!block)
		return AIR;
	return (block->getType());
}

void ChunkV2::sendFacesToDisplay()
{
	if (_isLoaded == false)
		return ;
	for (int i = 0; i < CHUNK_SIZEV2 * CHUNK_SIZEV2 * CHUNK_SIZEV2; i++)
	{
		if (_blocks[i])
			_blocks[i]->display();
	}
}