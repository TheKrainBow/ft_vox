#include "ChunkV2.hpp"
#include "World.hpp"

ChunkV2::ChunkV2(int x, int y, int z, World &world) : _world(world)
{
	_position = vec3(x, y, z);
	double *perlinMap = _world.getNoiseGenerator().noiseMap(x * CHUNK_SIZEV2, z * CHUNK_SIZEV2, CHUNK_SIZEV2);
	memcpy(_perlinMap, perlinMap, (sizeof(double) * CHUNK_SIZEV2 * CHUNK_SIZEV2));
	delete []perlinMap;
	bzero(_blocks, CHUNK_SIZEV2 * CHUNK_SIZEV2 * CHUNK_SIZEV2 * sizeof(char));
	load();
}

void ChunkV2::load()
{
	for (int y = 0; y < CHUNK_SIZEV2 ; y++)
	{
		for (int x = 0; x < CHUNK_SIZEV2 ; x++)
		{
			for (int z = 0; z < CHUNK_SIZEV2 ; z++)
			{
				double noise = _perlinMap[z * CHUNK_SIZEV2 + x];
				double remappedNoise = 100.0 + noise * 25.0;
				size_t maxHeight = (size_t)(remappedNoise);
				if (y + _position.y * CHUNK_SIZEV2 < maxHeight / 2)
					_blocks[x + (z * CHUNK_SIZEV2) + (y * CHUNK_SIZEV2 * CHUNK_SIZEV2)] = 'G';
				else if (y + _position.y * CHUNK_SIZEV2 < maxHeight)
					// _blocks[x + (z * CHUNK_SIZEV2) + (y * CHUNK_SIZEV2 * CHUNK_SIZEV2)] = 'D';
					_blocks[x + (z * CHUNK_SIZEV2) + (y * CHUNK_SIZEV2 * CHUNK_SIZEV2)] = 'G';
				else if (y + _position.y * CHUNK_SIZEV2 == maxHeight)
					// _blocks[x + (z * CHUNK_SIZEV2) + (y * CHUNK_SIZEV2 * CHUNK_SIZEV2)] = 'G';
					_blocks[x + (z * CHUNK_SIZEV2) + (y * CHUNK_SIZEV2 * CHUNK_SIZEV2)] = 'G';
				else
					_blocks[x + (z * CHUNK_SIZEV2) + (y * CHUNK_SIZEV2 * CHUNK_SIZEV2)] = 'A';
			}
		}
	}
}

ChunkV2::~ChunkV2()
{
}

char ChunkV2::getBlock(int x, int y, int z)
{
	if (x >= CHUNK_SIZEV2 || y >= CHUNK_SIZEV2 || z >= CHUNK_SIZEV2 || x < 0 || y < 0 || z < 0)
		return 'A';
	return _blocks[x + (z * CHUNK_SIZEV2) + (y * CHUNK_SIZEV2 * CHUNK_SIZEV2)];
}

void ChunkV2::addDirtBlock(int x, int y, int z)
{
	if (_world.getBlock(x, y - 1, z) == 'A')
		textManager.addTextureVertex(T_DIRT, DOWN	, x, y, z);
	if ((y == 255) || _blocks[x + z * CHUNK_SIZEV2 + (y + 1) * CHUNK_SIZEV2 * CHUNK_SIZEV2] == 'A')
		textManager.addTextureVertex(T_DIRT, UP		, x, y, z);
	if ((x == 0) || _blocks[(x - 1) + z * CHUNK_SIZEV2 + y * CHUNK_SIZEV2 * CHUNK_SIZEV2] == 'A')
		textManager.addTextureVertex(T_DIRT, NORTH	, x, y, z);
	if ((x == 15) || _blocks[(x + 1) + z * CHUNK_SIZEV2 + y * CHUNK_SIZEV2 * CHUNK_SIZEV2] == 'A')
		textManager.addTextureVertex(T_DIRT, SOUTH	, x, y, z);
	if ((z == 0) || _blocks[x + (z - 1) * CHUNK_SIZEV2 + y * CHUNK_SIZEV2 * CHUNK_SIZEV2] == 'A')
		textManager.addTextureVertex(T_DIRT, EAST	, x, y, z);
	if ((z == 15) || _blocks[x + (z + 1) * CHUNK_SIZEV2 + y * CHUNK_SIZEV2 * CHUNK_SIZEV2] == 'A')
		textManager.addTextureVertex(T_DIRT, WEST	, x, y, z);
}

void ChunkV2::addStoneBlock(int x, int y, int z)
{
	if ((y == 0) || _blocks[x + z * CHUNK_SIZEV2 + (y - 1) * CHUNK_SIZEV2 * CHUNK_SIZEV2] != 'A')
		textManager.addTextureVertex(T_STONE, DOWN	, x, y, z);
	if ((y == 255) || _blocks[x + z * CHUNK_SIZEV2 + (y + 1) * CHUNK_SIZEV2 * CHUNK_SIZEV2] != 'A')
		textManager.addTextureVertex(T_STONE, UP		, x, y, z);
	if ((z == 0) || _blocks[x + (z - 1) * CHUNK_SIZEV2 + y * CHUNK_SIZEV2 * CHUNK_SIZEV2] != 'A')
		textManager.addTextureVertex(T_STONE, NORTH	, x, y, z);
	if ((z == 15) || _blocks[x + (z + 1) * CHUNK_SIZEV2 + y * CHUNK_SIZEV2 * CHUNK_SIZEV2] != 'A')
		textManager.addTextureVertex(T_STONE, SOUTH	, x, y, z);
	if ((x == 0) || _blocks[(x - 1) + z * CHUNK_SIZEV2 + y * CHUNK_SIZEV2 * CHUNK_SIZEV2] != 'A')
		textManager.addTextureVertex(T_STONE, EAST	, x, y, z);
	if ((x == 15) || _blocks[(x + 1) + z * CHUNK_SIZEV2 + y * CHUNK_SIZEV2 * CHUNK_SIZEV2] != 'A')
		textManager.addTextureVertex(T_STONE, WEST	, x, y, z);
}

vec3 ChunkV2::getPosition()
{
	return _position;
}

void ChunkV2::addGrassBlock(int x, int y, int z)
{
	if (_world.getBlock(x, y - 1, z) == 'A')
		textManager.addTextureVertex(T_DIRT, DOWN	, x, y, z);
	if (_world.getBlock(x, y + 1, z) == 'A')
		textManager.addTextureVertex(T_GRASS_TOP, UP		, x, y, z);
	if (_world.getBlock(x, y, z - 1) == 'A')
		textManager.addTextureVertex(T_GRASS_SIDE, NORTH	, x, y, z);
	if (_world.getBlock(x, y, z + 1) == 'A')
		textManager.addTextureVertex(T_GRASS_SIDE, SOUTH	, x, y, z);
	if (_world.getBlock(x - 1, y, z) == 'A')
		textManager.addTextureVertex(T_GRASS_SIDE, EAST	, x, y, z);
	if (_world.getBlock(x + 1, y, z) == 'A')
		textManager.addTextureVertex(T_GRASS_SIDE, WEST	, x, y, z);
}

void ChunkV2::sendFacesToDisplay()
{
	for (int x = 0; x < CHUNK_SIZEV2; x++)
	{
		for (int y = 0; y < CHUNK_SIZEV2; y++)
		{
			for (int z = 0; z < CHUNK_SIZEV2; z++)
			{
				switch (_blocks[x + (z * CHUNK_SIZEV2) + (y * CHUNK_SIZEV2 * CHUNK_SIZEV2)])
				{
					case 'D':
						addDirtBlock(_position.x * CHUNK_SIZEV2 + x, _position.y * CHUNK_SIZEV2 + y, _position.z * CHUNK_SIZEV2 + z);
						break;
					case 'S':
						addStoneBlock(_position.x * CHUNK_SIZEV2 + x, _position.y * CHUNK_SIZEV2 + y, _position.z * CHUNK_SIZEV2 + z);
						break;
					case 'G':
						addGrassBlock(_position.x * CHUNK_SIZEV2 + x, _position.y * CHUNK_SIZEV2 + y, _position.z * CHUNK_SIZEV2 + z);
						break;
					default :
						break;
				}
			}
		}
	}
}