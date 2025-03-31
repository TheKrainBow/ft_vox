#include "SubChunk.hpp"

SubChunk::SubChunk(vec3 position, PerlinMap *perlinMap, Chunk &chunk, World &world, TextureManager &textManager) : _world(world), _chunk(chunk), _textManager(textManager)
{
	_position = position;
	_blocks.resize(CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE);
	std::fill(_blocks.begin(), _blocks.end(), 0);
	_perlinMap = &perlinMap->map;
	loadHeight();
	loadBiome();
}

void SubChunk::initGLBuffer()
{
	if (_hasBufferInitialized == true)
		return ;
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

    // Instance data (instancePositions)
    glBindBuffer(GL_ARRAY_BUFFER, _instanceVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(int) * _vertexData.size(), _vertexData.data(), GL_STATIC_DRAW);

    glVertexAttribIPointer(2, 1, GL_INT, sizeof(int), (void*)0); // Instance positions
    glEnableVertexAttribArray(2);
    glVertexAttribDivisor(2, 1); // Update once per instance

    glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
	_hasBufferInitialized = true;
}

void SubChunk::loadHeight()
{
	if (_loaded) return ;
	for (int y = 0; y < CHUNK_SIZE ; y++)
	{
		for (int z = 0; z < CHUNK_SIZE ; z++)
		{
			for (int x = 0; x < CHUNK_SIZE ; x++)
			{
				double height = (*_perlinMap)[z * CHUNK_SIZE + x];
				size_t maxHeight = (size_t)(height);
				if (y + _position.y * CHUNK_SIZE <= maxHeight)
					_blocks[x + (z * CHUNK_SIZE) + (y * CHUNK_SIZE * CHUNK_SIZE)] = STONE;
			}
		}
	}
	// for (int y = 1; y < 3 ; y++)
	// {
	// 	for (int x = 1; x < 4 ; x++)
	// 	{
	// 		for (int z = 1; z < 5 ; z++)
	// 		{
	// 			// double height = _perlinMap[z * CHUNK_SIZE + x];
	// 			// height = 100;
	// 			// size_t maxHeight = (size_t)(height);
	// 			// if (y + _position.y * CHUNK_SIZE <= maxHeight)
	// 			_blocks[x + (z * CHUNK_SIZE) + (y * CHUNK_SIZE * CHUNK_SIZE)] = STONE;
	// 		}
	// 	}
	// }
	_loaded = true;
}

vec2 SubChunk::getBorderWarping(double x, double z, NoiseGenerator &noise_gen) const
{
	double noiseX = noise_gen.noise(x, z);
	double noiseY = noise_gen.noise(z, x);
	vec2 offset;
	offset.x = noiseX * 15.0;
	offset.y = noiseY * 15.0;
	return offset;
}

void SubChunk::loadOcean(int x, int z, size_t ground)
{
	int y;
	for (y = OCEAN_HEIGHT; y > (int)ground; y--)
		setBlock(vec3(x, y - _position.y * CHUNK_SIZE, z), WATER);
	setBlock(vec3(x, y - _position.y * CHUNK_SIZE, z), SAND);
	int i;
	for (i = -1; i > -4; i--)
		setBlock(vec3(x, y + i - _position.y * CHUNK_SIZE, z), SAND);
	for (; i > -8; i--)
		setBlock(vec3(x, y + i - _position.y * CHUNK_SIZE, z), DIRT);
}

void SubChunk::loadPlaine(int x, int z, size_t ground)
{
	setBlock(vec3(x, ground - _position.y * CHUNK_SIZE, z), GRASS);
	for (int i = -1; i > -5; i--)
		setBlock(vec3(x, ground + i - _position.y * CHUNK_SIZE, z), DIRT);
}

void SubChunk::loadBiome()
{
	for (int x = 0; x < CHUNK_SIZE ; x++)
	{
		for (int z = 0; z < CHUNK_SIZE ; z++)
		{
			size_t ground = (*_perlinMap)[z * CHUNK_SIZE + x];
			if (ground <= OCEAN_HEIGHT)
				loadOcean(x, z, ground);
			else if (ground)
				loadPlaine(x, z, ground);
		}
	}
}

SubChunk::~SubChunk()
{
	_loaded = false;
}

void SubChunk::setBlock(vec3 position, char block)
{
	int x = position.x;
	int y = position.y;
	int z = position.z;

	if (x >= CHUNK_SIZE || y >= CHUNK_SIZE || z >= CHUNK_SIZE || x < 0 || y < 0 || z < 0)
		return ;
	_blocks[x + (z * CHUNK_SIZE) + (y * CHUNK_SIZE * CHUNK_SIZE)] = block;
}

char SubChunk::getBlock(vec3 position)
{
	int x = position.x;
	int y = position.y;
	int z = position.z;

	if (x >= CHUNK_SIZE || y >= CHUNK_SIZE || z >= CHUNK_SIZE || x < 0 || y < 0 || z < 0)
		return 0;
	return _blocks[x + (z * CHUNK_SIZE) + y * CHUNK_SIZE * CHUNK_SIZE];
}

void SubChunk::addDownFace(BlockType current, vec3 position, TextureType texture)
{
	char block = 0;
	if (position.y > 0)
		block = getBlock({position.x, position.y - 1, position.z});
	else
	{
		SubChunk *underChunk = nullptr;
		underChunk = _chunk.getSubChunk(_position.y - 1);
		if (!underChunk)
			block = '/';
		else
			block = underChunk->getBlock({position.x, CHUNK_SIZE - 1, position.z});
	}
	if (faceDisplayCondition(current, block))
		addFace(position, DOWN, texture);
}

void SubChunk::addUpFace(BlockType current, vec3 position, TextureType texture)
{
	char block = 0;
	if (position.y != CHUNK_SIZE - 1)
		block = getBlock({position.x, position.y + 1, position.z});
	else
	{
		SubChunk *overChunk = _chunk.getSubChunk(_position.y + 1);
		if (overChunk)
			block = overChunk->getBlock({position.x, 0, position.z});
	}
	if (faceDisplayCondition(current, block))
		addFace(position, UP, texture);
}

void SubChunk::addNorthFace(BlockType current, vec3 position, TextureType texture)
{
	int x = _position.x * CHUNK_SIZE + position.x;
	int y = _position.y * CHUNK_SIZE + position.y;
	int z = _position.z * CHUNK_SIZE + position.z;
	(void)current;
	char block = 0;
	if (position.z != 0)
		block = getBlock({position.x, position.y, position.z - 1});
	else
		block = _world.getBlock(vec3(x, y, z - 1));
	if (faceDisplayCondition(current, block))
		addFace(position, NORTH, texture);
}

void SubChunk::addSouthFace(BlockType current, vec3 position, TextureType texture)
{
	int x = _position.x * CHUNK_SIZE + position.x;
	int y = _position.y * CHUNK_SIZE + position.y;
	int z = _position.z * CHUNK_SIZE + position.z;
	(void)current;

	char block = 0;
	if (position.z != CHUNK_SIZE - 1)
		block = getBlock({position.x, position.y, position.z + 1});
	else
		block = _world.getBlock(vec3(x, y, z + 1));
	if (faceDisplayCondition(current, block))
		addFace(position, SOUTH, texture);
}

void SubChunk::addWestFace(BlockType current, vec3 position, TextureType texture)
{
	int x = _position.x * CHUNK_SIZE + position.x;
	int y = _position.y * CHUNK_SIZE + position.y;
	int z = _position.z * CHUNK_SIZE + position.z;
	(void)current;

	char block = 0;
	if (position.x != 0)
		block = getBlock({position.x - 1, position.y, position.z});
	else
		block = _world.getBlock(vec3(x - 1, y, z));
	if (faceDisplayCondition(current, block))
		addFace(position, WEST, texture);
}

void SubChunk::addEastFace(BlockType current, vec3 position, TextureType texture)
{
	int x = _position.x * CHUNK_SIZE + position.x;
	int y = _position.y * CHUNK_SIZE + position.y;
	int z = _position.z * CHUNK_SIZE + position.z;
	(void)current;

	char block = 0;
	if (position.x != CHUNK_SIZE - 1)
		block = getBlock({position.x + 1, position.y, position.z});
	else
		block = _world.getBlock(vec3(x + 1, y, z));
	if (faceDisplayCondition(current, block))
		addFace(position, EAST, texture);
}

void SubChunk::addBlock(BlockType block, vec3 position, TextureType down, TextureType up, TextureType north, TextureType south, TextureType east, TextureType west)
{
	addUpFace(block, position, up);
	if (block == WATER)
		return;
	addDownFace(block, position, down);
	addNorthFace(block, position, north);
	addSouthFace(block, position, south);
	addWestFace(block, position, west);
	addEastFace(block, position, east);
}

vec3 SubChunk::getPosition()
{
	return _position;
}

void SubChunk::clearFaces() {
	std::cout << "Cleared faces" << std::endl;
	for (int i = 0; i < 6; i++)
		_faces[i].clear();
	_vertexData.clear();
	_hasSentFaces = false;
}

void SubChunk::sendFacesToDisplay()
{
	if (_hasSentFaces == true)
		return ;
	// 	clearFaces();
	for (int x = 0; x < CHUNK_SIZE; x++)
	{
		for (int y = 0; y < CHUNK_SIZE; y++)
		{
			for (int z = 0; z < CHUNK_SIZE; z++)
			{
				switch (_blocks[x + (z * CHUNK_SIZE) + (y * CHUNK_SIZE * CHUNK_SIZE)])
				{
					case DIRT:
						addBlock(DIRT, vec3(x, y, z), T_DIRT, T_DIRT, T_DIRT, T_DIRT, T_DIRT, T_DIRT);
						break;
					case STONE:
						addBlock(STONE, vec3(x, y, z), T_STONE, T_STONE, T_STONE, T_STONE, T_STONE, T_STONE);
						break;
					case GRASS:
						addBlock(GRASS, vec3(x, y, z), T_DIRT, T_GRASS_TOP, T_GRASS_SIDE, T_GRASS_SIDE, T_GRASS_SIDE, T_GRASS_SIDE);
						break;
					case SAND:
						addBlock(SAND, vec3(x, y, z), T_SAND, T_SAND, T_SAND, T_SAND, T_SAND, T_SAND);
						break;
					case WATER:
						addBlock(WATER, vec3(x, y, z), T_WATER, T_WATER, T_WATER, T_WATER, T_WATER, T_WATER);
						break;
					default :
						break;
				}
			}
		}
	}
	processFaces();
	_hasSentFaces = true;
}

void SubChunk::addTextureVertex(Face face)
{
	int x = face.position.x;
	int y = face.position.y;
	int z = face.position.z;
	int textureID = face.texture;
	int direction = face.direction;
	if (x < 0 || y < 0 || z < 0 || x >= CHUNK_SIZE || y >= CHUNK_SIZE || z >= CHUNK_SIZE || direction >= 6)
		return ;
	int newVertex = 0;
	int lengthX = face.size.x - 1;
	int lengthY = face.size.y - 1;

	if (face.direction == EAST || face.direction == WEST)
	{
		lengthX = face.size.y - 1;
		lengthY = face.size.x - 1;
	}
	
	newVertex |= (x & 0x1F) << 0;			// 5 bits for x
	newVertex |= (y & 0x1F) << 5;			// 5 bits for y
	newVertex |= (z & 0x1F) << 10;			// 5 bits for z
	newVertex |= (direction & 0x07) << 15;	// 3 bits for direction
	newVertex |= (lengthX & 0x1F) << 18;	// 5 bits for lengthX
	newVertex |= (lengthY & 0x1F) << 23;	// 5 bits for lengthY
	newVertex |= (textureID & 0x0F) << 28;	// 4 bits for textureID
	_vertexData.push_back(newVertex);
}

int SubChunk::display(void)
{
	if (_hasBufferInitialized == false)
		initGLBuffer();
    glBindVertexArray(_vao);
	glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, _vertexData.size());
	glBindVertexArray(0);
    return (_vertexData.size() * 2);
}

void SubChunk::processFaces()
{
	processUpVertex();
	processDownVertex();
	processNorthVertex();
	processSouthVertex();
	processEastVertex();
	processWestVertex();
}
