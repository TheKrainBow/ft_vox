#include "Chunk.hpp"
#include "World.hpp"
#include "globals.hpp"

Chunk::Chunk(int x, int y, int z, NoiseGenerator::PerlinMap *perlinMap, World &world) : _world(world)
{
	_position = vec3(x, y, z);
	_blocks.resize(CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE);
	glGenVertexArrays(1, &_vao);
	glGenBuffers(1, &_vbo);
	memcpy(_perlinMap, perlinMap->map, (sizeof(double) * perlinMap->size * perlinMap->size));
	bzero(_blocks.data(), _blocks.size());
	loadHeight();
	loadBiome();
}

void Chunk::loadHeight()
{
	if (loaded) return ;
	loaded = true;
	// int maxHeight = (_position.x + _position.z) * 32;
	// _blocks[0] = 'S';
	_blocks[(5 * CHUNK_SIZE * CHUNK_SIZE)] = 'S';
	// for (int y = 0; y < CHUNK_SIZE ; y++)
	// {
	// 	for (int x = 0; x < CHUNK_SIZE ; x++)
	// 	{
	// 		for (int z = 0; z < CHUNK_SIZE ; z++)
	// 		{
	// 			if (y + _position.y * CHUNK_SIZE <= maxHeight)
	// 				_blocks[x + (z * CHUNK_SIZE) + (y * CHUNK_SIZE * CHUNK_SIZE)] = 'S';
	// 		}
	// 	}
	// }
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
	loaded = false;
}

char Chunk::getBlock(int x, int y, int z)
{
	if (x >= CHUNK_SIZE || y >= CHUNK_SIZE || z >= CHUNK_SIZE || x < 0 || y < 0 || z < 0)
		return 'D';
	return _blocks[x + (z * CHUNK_SIZE) + (y * CHUNK_SIZE * CHUNK_SIZE)];
}

void Chunk::addBlock(int blockX, int blockY, int blockZ, TextureType down, TextureType up, TextureType north, TextureType south, TextureType east, TextureType west)
{
	(void)up;
	(void)down;
	(void)east;
	(void)west;
	(void)north;
	(void)south;
	int x = _position.x * CHUNK_SIZE + blockX;
	int y = _position.y * CHUNK_SIZE + blockY;
	int z = _position.z * CHUNK_SIZE + blockZ;
	// if ((blockY == 0 && _world.getBlock(x, y - 1, z) == 0) || ((blockY != 0 && _blocks[blockX + (blockZ * CHUNK_SIZE) + ((blockY - 1) * CHUNK_SIZE * CHUNK_SIZE)] == 0)))
	addTextureVertex(x, y, z, EAST, down, 0, 0);
	addTextureVertex(x + 1, y, z, EAST, down, 0, 1);
	addTextureVertex(x + 1, y + 1, z, EAST, down, 1, 1);
	addTextureVertex(x, y, z, EAST, down, 0, 0);
	addTextureVertex(x + 1, y + 1, z, EAST, down, 1, 1);
	addTextureVertex(x, y + 1, z, EAST, down, 0, 1);
	// if ((blockY == (CHUNK_SIZE - 1) && _world.getBlock(x, y + 1, z) == 0) || ((blockY != (CHUNK_SIZE - 1) && _blocks[blockX + (blockZ * CHUNK_SIZE) + ((blockY + 1) * CHUNK_SIZE * CHUNK_SIZE)] == 0)))
		// addTextureVertex(x, y, z, UP, up);
	// if ((blockZ == 0 && _world.getBlock(x, y, z - 1) == 0) || ((blockZ != 0 && _blocks[blockX + ((blockZ - 1) * CHUNK_SIZE) + (blockY * CHUNK_SIZE * CHUNK_SIZE)] == 0)))
		// addTextureVertex(x, y, z, NORTH, north);
	// if ((blockZ == (CHUNK_SIZE - 1) && _world.getBlock(x, y, z + 1) == 0) || ((blockZ != (CHUNK_SIZE - 1) && _blocks[blockX + ((blockZ + 1) * CHUNK_SIZE) + (blockY * CHUNK_SIZE * CHUNK_SIZE)] == 0)))
		// addTextureVertex(x, y, z, SOUTH, south);
	// if ((blockX == 0 && _world.getBlock(x - 1, y, z) == 0) || ((blockX != 0 && _blocks[(blockX - 1) + (blockZ * CHUNK_SIZE) + (blockY * CHUNK_SIZE * CHUNK_SIZE)] == 0)))
		// addTextureVertex(x, y, z, EAST, east);
	// if ((blockX == (CHUNK_SIZE - 1) && _world.getBlock(x + 1, y, z) == 0) || ((blockX != (CHUNK_SIZE - 1) && _blocks[(blockX + 1) + (blockZ * CHUNK_SIZE) + (blockY * CHUNK_SIZE * CHUNK_SIZE)] == 0)))
		// addTextureVertex(x, y, z, WEST, west);
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
}

void Chunk::addTextureVertex(int x, int y, int z, unsigned int direction, int textureID, int u, int v)
{
	(void)u;
	(void)v;
	int newVertex = 0;
	if (x < 0 || y < 0 || z < 0 || x >= CHUNK_SIZE || y >= CHUNK_SIZE || z >= CHUNK_SIZE || direction >= 6)
		return ;
		
	newVertex |= (x & 0x3F) << 0;   // Use 6 bits for x
	newVertex |= (y & 0x3F) << 6;   // Use 6 bits for y
	newVertex |= (z & 0x3F) << 12;  // Use 6 bits for z
	newVertex |= (direction & 0x07) << 18;
	newVertex |= (textureID & 0x7F) << 21;
	newVertex |= (u & 0x01) << 28;  // Store U in bit 28
	newVertex |= (v & 0x01) << 29;  // Store V in bit 29

	_vertexData.push_back(newVertex);
}

void Chunk::setupBuffers()
{
    if (_vertexData.empty()) return;

    glBindVertexArray(_vao);

    glBindBuffer(GL_ARRAY_BUFFER, _vbo);
    glBufferData(GL_ARRAY_BUFFER, _vertexData.size() * sizeof(int), _vertexData.data(), GL_STATIC_DRAW);

    // Assuming that 'vertex' stores all the data packed into a single integer
    glVertexAttribPointer(0, 1, GL_INT, GL_FALSE, sizeof(int), (void*)0);  // Attribute 0 for vertex data
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

int Chunk::display(void)
{
    setupBuffers();

    glBindVertexArray(_vao);
    // Draw triangles (each triangle has 3 vertices, so we need to divide by 3)
    glDrawArrays(GL_TRIANGLES, 0, _vertexData.size());  // Adjust based on number of vertices
    glBindVertexArray(0);
    return (_vertexData.size() / 3);
}