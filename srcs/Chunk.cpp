#include "Chunk.hpp"
#include "World.hpp"
#include "globals.hpp"

Chunk::Chunk(int x, int y, int z, NoiseGenerator::PerlinMap *perlinMap, World &world) : _world(world)
{
	_position = vec3(x, y, z);
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
	for (int y = 0; y < CHUNK_SIZE ; y++)
	{
		for (int x = 0; x < CHUNK_SIZE ; x++)
		{
			for (int z = 0; z < CHUNK_SIZE ; z++)
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

	if ((blockY == 0 && _world.getBlock(x, y - 1, z) == 0) || ((blockY != 0 && _blocks[blockX + (blockZ * CHUNK_SIZE) + ((blockY - 1) * CHUNK_SIZE * CHUNK_SIZE)] == 0)))
		addTextureVertex(blockX, blockY, blockZ, DOWN, down);
	if ((blockY == (CHUNK_SIZE - 1) && _world.getBlock(x, y + 1, z) == 0) || ((blockY != (CHUNK_SIZE - 1) && _blocks[blockX + (blockZ * CHUNK_SIZE) + ((blockY + 1) * CHUNK_SIZE * CHUNK_SIZE)] == 0)))
		addTextureVertex(blockX, blockY, blockZ, UP, up);
	if ((blockZ == 0 && _world.getBlock(x, y, z - 1) == 0) || ((blockZ != 0 && _blocks[blockX + ((blockZ - 1) * CHUNK_SIZE) + (blockY * CHUNK_SIZE * CHUNK_SIZE)] == 0)))
		addTextureVertex(blockX, blockY, blockZ, NORTH, north);
	if ((blockZ == (CHUNK_SIZE - 1) && _world.getBlock(x, y, z + 1) == 0) || ((blockZ != (CHUNK_SIZE - 1) && _blocks[blockX + ((blockZ + 1) * CHUNK_SIZE) + (blockY * CHUNK_SIZE * CHUNK_SIZE)] == 0)))
		addTextureVertex(blockX, blockY, blockZ, SOUTH, south);
	if ((blockX == 0 && _world.getBlock(x - 1, y, z) == 0) || ((blockX != 0 && _blocks[(blockX - 1) + (blockZ * CHUNK_SIZE) + (blockY * CHUNK_SIZE * CHUNK_SIZE)] == 0)))
		addTextureVertex(blockX, blockY, blockZ, WEST, west);
	if ((blockX == (CHUNK_SIZE - 1) && _world.getBlock(x + 1, y, z) == 0) || ((blockX != (CHUNK_SIZE - 1) && _blocks[(blockX + 1) + (blockZ * CHUNK_SIZE) + (blockY * CHUNK_SIZE * CHUNK_SIZE)] == 0)))
		addTextureVertex(blockX, blockY, blockZ, EAST, east);
}

vec3 Chunk::getPosition()
{
	return _position;
}


vec3 Chunk::getWorldPosition()
{
	vec3 worldPos;
	worldPos.x = _position.x * CHUNK_SIZE;
	worldPos.y = _position.y * CHUNK_SIZE;
	worldPos.z = _position.z * CHUNK_SIZE;
	return worldPos;
}

void Chunk::sendFacesToDisplay()
{
	if (_hasSentFaces == true)
		return ;
	_bufferStartIndex = _world.getVertexSize();
	for (int x = 0; x < CHUNK_SIZE; x++)
	{
		for (int y = 0; y < CHUNK_SIZE; y++)
		{
			for (int z = 0; z < CHUNK_SIZE; z++)
			{
				switch (_blocks[x + (z * CHUNK_SIZE) + (y * CHUNK_SIZE * CHUNK_SIZE)])
				{
					case 'D':
						addBlock(x, y, z, T_DIRT, T_DIRT, T_DIRT, T_DIRT, T_DIRT, T_DIRT);
						break;
					case 'S':
						addBlock(x, y, z, T_STONE, T_STONE, T_STONE, T_STONE, T_STONE, T_STONE);
						break;
					case 'G':
						addBlock(x, y, z, T_DIRT, T_GRASS_TOP, T_GRASS_SIDE, T_GRASS_SIDE, T_GRASS_SIDE, T_GRASS_SIDE);
						break;
					default :
						break;
				}
			}
		}
	}
	_hasSentFaces = true;
}

void Chunk::addTextureVertex(int x, int y, int z, int direction, int textureID)
{
	if (x < 0 || y < 0 || z < 0 || x >= CHUNK_SIZE || y >= CHUNK_SIZE || z >= CHUNK_SIZE || direction >= 6)
		return ;
		
	int newVertex = 0;
	newVertex |= (x & 0x1F) << 0;
	newVertex |= (y & 0x1F) << 5;
	newVertex |= (z & 0x1F) << 10;
	newVertex |= (direction & 0x07) << 15;
	newVertex |= (textureID & 0x7F) << 18;
	_world.addVertex(newVertex, getWorldPosition());
	_bufferLenght++;
}

int Chunk::getStartIndex(void)
{
	return (_bufferStartIndex);
}

int Chunk::getBufferLenght(void)
{
	return (_bufferLenght);
}