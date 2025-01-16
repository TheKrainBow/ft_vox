#include "Chunk.hpp"
#include "World.hpp"
#include "globals.hpp"

Chunk::Chunk(int x, int y, int z, World &world) : _world(world)
{
	_position = vec3(x, y, z);
	_blocks.resize(CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE);
	double *perlinMap = _world.getNoiseGenerator().noiseMap(x * CHUNK_SIZE, z * CHUNK_SIZE, CHUNK_SIZE);
	memcpy(_perlinMap, perlinMap, (sizeof(double) * CHUNK_SIZE * CHUNK_SIZE));
	delete [] perlinMap;
	bzero(_blocks.data(), _blocks.size());
	load();
}

void Chunk::load()
{
	if (loaded) return ;
	loaded = true;
	for (int y = 0; y < CHUNK_SIZE ; y++)
	{
		for (int x = 0; x < CHUNK_SIZE ; x++)
		{
			for (int z = 0; z < CHUNK_SIZE ; z++)
			{
				double noise = _perlinMap[z * CHUNK_SIZE + x];
				double remappedNoise = 100.0 + noise * 25.0;
				size_t maxHeight = (size_t)(remappedNoise);
				if (y + _position.y * CHUNK_SIZE < maxHeight / 2)
					_blocks[x + (z * CHUNK_SIZE) + (y * CHUNK_SIZE * CHUNK_SIZE)] = 'S';
				else if (y + _position.y * CHUNK_SIZE < maxHeight)
					_blocks[x + (z * CHUNK_SIZE) + (y * CHUNK_SIZE * CHUNK_SIZE)] = 'D';
				else if (y + _position.y * CHUNK_SIZE == maxHeight)
					_blocks[x + (z * CHUNK_SIZE) + (y * CHUNK_SIZE * CHUNK_SIZE)] = 'G';
				else
					_blocks[x + (z * CHUNK_SIZE) + (y * CHUNK_SIZE * CHUNK_SIZE)] = 'A';
			}
		}
	}
}

Chunk::~Chunk()
{
	loaded = false;
}

char Chunk::getBlock(int x, int y, int z)
{
	if (x >= CHUNK_SIZE || y >= CHUNK_SIZE || z >= CHUNK_SIZE || x < 0 || y < 0 || z < 0)
		return 'D';
	return _blocks[x + (z * CHUNK_SIZE) + (y * CHUNK_SIZE * CHUNK_SIZE)];
}

void Chunk::addDirtBlock(int x, int y, int z)
{
	if (_world.getBlock(x, y - 1, z) == 'A')
		textManager.addTextureVertex(T_DIRT, DOWN	, x, y, z);
	if (_world.getBlock(x, y + 1, z) == 'A')
		textManager.addTextureVertex(T_DIRT, UP	, x, y, z);
	if (_world.getBlock(x, y, z - 1) == 'A')
		textManager.addTextureVertex(T_DIRT, NORTH	, x, y, z);
	if (_world.getBlock(x, y, z + 1) == 'A')
		textManager.addTextureVertex(T_DIRT, SOUTH	, x, y, z);
	if (_world.getBlock(x - 1, y, z) == 'A')
		textManager.addTextureVertex(T_DIRT, EAST	, x, y, z);
	if (_world.getBlock(x + 1, y, z) == 'A')
		textManager.addTextureVertex(T_DIRT, WEST	, x, y, z);
}

void Chunk::addStoneBlock(int x, int y, int z)
{
	if (_world.getBlock(x, y - 1, z) == 'A')
		textManager.addTextureVertex(T_STONE, DOWN	, x, y, z);
	if (_world.getBlock(x, y + 1, z) == 'A')
		textManager.addTextureVertex(T_STONE, UP	, x, y, z);
	if (_world.getBlock(x, y, z - 1) == 'A')
		textManager.addTextureVertex(T_STONE, NORTH	, x, y, z);
	if (_world.getBlock(x, y, z + 1) == 'A')
		textManager.addTextureVertex(T_STONE, SOUTH	, x, y, z);
	if (_world.getBlock(x - 1, y, z) == 'A')
		textManager.addTextureVertex(T_STONE, EAST	, x, y, z);
	if (_world.getBlock(x + 1, y, z) == 'A')
		textManager.addTextureVertex(T_STONE, WEST	, x, y, z);
}

void Chunk::addGrassBlock(int x, int y, int z)
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

vec3 Chunk::getPosition()
{
	return _position;
}

void Chunk::sendFacesToDisplay()
{
	for (int x = 0; x < CHUNK_SIZE; x++)
	{
		for (int y = 0; y < CHUNK_SIZE; y++)
		{
			for (int z = 0; z < CHUNK_SIZE; z++)
			{
				switch (_blocks[x + (z * CHUNK_SIZE) + (y * CHUNK_SIZE * CHUNK_SIZE)])
				{
					case 'D':
						addDirtBlock(_position.x * CHUNK_SIZE + x, _position.y * CHUNK_SIZE + y, _position.z * CHUNK_SIZE + z);
						break;
					case 'S':
						addStoneBlock(_position.x * CHUNK_SIZE + x, _position.y * CHUNK_SIZE + y, _position.z * CHUNK_SIZE + z);
						break;
					case 'G':
						addGrassBlock(_position.x * CHUNK_SIZE + x, _position.y * CHUNK_SIZE + y, _position.z * CHUNK_SIZE + z);
						break;
					default :
						break;
				}
			}
		}
	}
}