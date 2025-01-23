#include "Chunk.hpp"
#include "World.hpp"
#include "globals.hpp"

Chunk::Chunk(int x, int y, int z, NoiseGenerator::PerlinMap *perlinMap, World &world) : _world(world)
{
	_position = vec3(x, y, z);
	_blocks.resize(CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE);
	glGenVertexArrays(1, &_vao);
	glGenBuffers(1, &_vbo);
	glGenBuffers(1, &_instanceVBO);


    GLfloat vertices[] = {
        0, 0, 0, _position.x * CHUNK_SIZE, _position.y * CHUNK_SIZE, _position.z * CHUNK_SIZE,
        1, 0, 0, _position.x * CHUNK_SIZE, _position.y * CHUNK_SIZE, _position.z * CHUNK_SIZE,
        0, 1, 0, _position.x * CHUNK_SIZE, _position.y * CHUNK_SIZE, _position.z * CHUNK_SIZE,
        1, 1, 0, _position.x * CHUNK_SIZE, _position.y * CHUNK_SIZE, _position.z * CHUNK_SIZE,
    };

    glBindVertexArray(_vao);
    glBindBuffer(GL_ARRAY_BUFFER, _vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), (void*)0); // Positions
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), (void*)(3 * sizeof(GLfloat))); // Offset
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);

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
		addTextureVertex(blockX, y, z, DOWN, down);
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

void Chunk::sendFacesToDisplay()
{
	if (_hasSentFaces == true)
		return ;
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
	setupBuffers();
	_hasSentFaces = true;
}

void Chunk::addTextureVertex(int x, int y, int z, int direction, int textureID)
{
	(void)textureID;
	(void)direction;
	if (x < 0 || y < 0 || z < 0 || x >= CHUNK_SIZE || y >= CHUNK_SIZE || z >= CHUNK_SIZE || direction >= 6)
		return ;
		
	int newVertex = 0;
	newVertex |= (x & 0x1F) << 0;
	newVertex |= (y & 0x1F) << 5;
	newVertex |= (z & 0x1F) << 10;
	newVertex |= (direction & 0x07) << 15;
	newVertex |= (textureID & 0x7F) << 18;
	_vertexData.push_back(newVertex);
}

void Chunk::setupBuffers() {

    if (_vertexData.empty()) return;

    glBindVertexArray(_vao);

    // Instance data (instancePositions)
    glBindBuffer(GL_ARRAY_BUFFER, _instanceVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(int) * _vertexData.size(), _vertexData.data(), GL_STATIC_DRAW);

    glVertexAttribIPointer(2, 1, GL_INT, sizeof(int), (void*)0); // Instance positions
    glEnableVertexAttribArray(2);
    glVertexAttribDivisor(2, 1); // Update once per instance

    glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
}

int Chunk::display(void)
{
	if (_vertexData.empty())
		return 0;
    glBindVertexArray(_vao);
	glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, _vertexData.size());
	glBindVertexArray(0);
    return (_vertexData.size() * 2);
}
