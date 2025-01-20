#include "Chunk.hpp"
#include "World.hpp"
#include "globals.hpp"

Chunk::Chunk(int x, int y, int z, NoiseGenerator::PerlinMap *perlinMap, World &world, int resolution) : _world(world)
{
	_position = vec3(x, y, z);
	_resolution = resolution;
	_blocks.resize(CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE);
	memcpy(_perlinMap, perlinMap->map, (sizeof(double) * perlinMap->size * perlinMap->size));
	bzero(_blocks.data(), _blocks.size());
	loadHeight();
	loadBiome();
}

void Chunk::loadHeight()
{
	if (_loaded) return ;
	_loaded = true;
	for (int y = 0; y < CHUNK_SIZE ; y += _resolution)
	{
		for (int x = 0; x < CHUNK_SIZE ; x += _resolution)
		{
			for (int z = 0; z < CHUNK_SIZE ; z += _resolution)
			{
				double height = _perlinMap[z * CHUNK_SIZE + x];
				size_t maxHeight = (size_t)(height);
				if (y + _position.y * CHUNK_SIZE <= maxHeight)
					_blocks[x + (z * CHUNK_SIZE) + (y * CHUNK_SIZE * CHUNK_SIZE)] = 'S';
			}
		}
	}
}
void Chunk::loadBiome()
{
	for (int x = 0; x < CHUNK_SIZE ; x++)
	{
		for (int z = 0; z < CHUNK_SIZE ; z++)
		{
			bool surfaceFound = false;
			for (int y = CHUNK_SIZE - 1; y >= 0; y--)
			{
				int index = x + (z * CHUNK_SIZE) + (y * CHUNK_SIZE * CHUNK_SIZE);
				if (!surfaceFound && _blocks[index] == 'S')
				{
					_blocks[index] = 'G';
					surfaceFound = true;
					for (int dirtLayer = 1; dirtLayer <= 5; dirtLayer++)
					{
						int dirtIndex = x + (z * CHUNK_SIZE) + ((y - dirtLayer) * CHUNK_SIZE * CHUNK_SIZE);
						if (y - dirtLayer >= 0 && _blocks[dirtIndex] == 'S')
							_blocks[dirtIndex] = 'D';
						else
							break ;
					}
				}
			}
		}
	}
}

Chunk::~Chunk()
{
	_loaded = false;
}

char Chunk::getBlock(int x, int y, int z)
{
	if (x >= CHUNK_SIZE || y >= CHUNK_SIZE || z >= CHUNK_SIZE || x < 0 || y < 0 || z < 0)
		return 'D';
	return _blocks[x + (z * CHUNK_SIZE) + (y * CHUNK_SIZE * CHUNK_SIZE)];
}

void Chunk::addBlock(int blockX, int blockY, int blockZ, TextureType down, TextureType up, TextureType north, TextureType south, TextureType east, TextureType west)
{
	int x = _position.x * CHUNK_SIZE + blockX;
	int y = _position.y * CHUNK_SIZE + blockY;
	int z = _position.z * CHUNK_SIZE + blockZ;
	if ((blockY <= 0 && !_world.getBlock(x, y - _resolution, z)) || ((blockY != 0 && !_blocks[blockX + (blockZ * CHUNK_SIZE) + ((blockY - _resolution) * CHUNK_SIZE * CHUNK_SIZE)])))
		textManager.addTextureVertex(down, DOWN, x, y, z, _resolution);
	if ((blockY >= CHUNK_SIZE - _resolution && !_world.getBlock(x, y + _resolution, z)) || ((blockY < CHUNK_SIZE - _resolution && !_blocks[blockX + (blockZ * CHUNK_SIZE) + ((blockY + _resolution) * CHUNK_SIZE * CHUNK_SIZE)])))
		textManager.addTextureVertex(up, UP, x, y, z, _resolution);
	if ((blockZ <= 0 && !_world.getBlock(x, y, z - _resolution)) || ((blockZ != 0 && !_blocks[blockX + ((blockZ - _resolution) * CHUNK_SIZE) + (blockY * CHUNK_SIZE * CHUNK_SIZE)])))
		textManager.addTextureVertex(north, NORTH, x, y, z, _resolution);
	if ((blockZ >= CHUNK_SIZE - _resolution && !_world.getBlock(x, y, z + _resolution)) || ((blockZ < CHUNK_SIZE - _resolution && !_blocks[blockX + ((blockZ + _resolution) * CHUNK_SIZE) + (blockY * CHUNK_SIZE * CHUNK_SIZE)])))
		textManager.addTextureVertex(south, SOUTH, x, y, z, _resolution);
	if ((blockX <= 0 && !_world.getBlock(x - _resolution, y, z)) || ((blockX != 0 && !_blocks[(blockX - _resolution) + (blockZ * CHUNK_SIZE) + (blockY * CHUNK_SIZE * CHUNK_SIZE)])))
		textManager.addTextureVertex(east, EAST, x, y, z, _resolution);
	if ((blockX >= CHUNK_SIZE - _resolution && !_world.getBlock(x + _resolution, y, z)) || ((blockX < CHUNK_SIZE - _resolution && !_blocks[(blockX + _resolution) + (blockZ * CHUNK_SIZE) + (blockY * CHUNK_SIZE * CHUNK_SIZE)])))
		textManager.addTextureVertex(west, WEST, x, y, z, _resolution);
}

vec3 Chunk::getPosition()
{
	return _position;
}

void Chunk::sendFacesToDisplay()
{
	for (int x = 0; x < CHUNK_SIZE; x += _resolution)
	{
		for (int y = 0; y < CHUNK_SIZE; y += _resolution)
		{
			for (int z = 0; z < CHUNK_SIZE; z += _resolution)
			{
				switch (_blocks[x + (z * CHUNK_SIZE) + (y * CHUNK_SIZE * CHUNK_SIZE)])
				{
					case 0:
						break;
					case 'D':
						addBlock(x, y, z, T_DIRT, T_DIRT, T_DIRT, T_DIRT, T_DIRT, T_DIRT);
						break;
					case 'S':
						addBlock(x, y, z, T_STONE, T_STONE, T_STONE, T_STONE, T_STONE, T_STONE);
						break;
					case 'G':
						addBlock(x, y, z, T_GRASS_TOP, T_GRASS_TOP, T_GRASS_SIDE, T_GRASS_SIDE, T_GRASS_SIDE, T_GRASS_SIDE);
						break;
					default :
						break;
				}
			}
		}
	}
}

void Chunk::updateResolution(int newResolution)
{
	std::cout << "Upgrading resolution from " << _resolution << " to " << newResolution << std::endl;
	_resolution = newResolution;
	_loaded = false;
	bzero(_blocks.data(), _blocks.size());
	loadHeight();
	loadBiome();
}

int Chunk::getResolution()
{
	return (_resolution);
}