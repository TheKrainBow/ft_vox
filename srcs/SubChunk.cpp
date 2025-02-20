#include "SubChunk.hpp"

SubChunk::SubChunk(vec3 position, PerlinMap *perlinMap, World &world, TextureManager &textManager) : _world(world), _textManager(textManager)
{
	_position = position;
	_blocks.resize(CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE);
	memcpy(_perlinMap, perlinMap->map, (sizeof(double) * perlinMap->size * perlinMap->size));
	bzero(_blocks.data(), _blocks.size());
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
	// 			_blocks[x + (z * CHUNK_SIZE) + (y * CHUNK_SIZE * CHUNK_SIZE)] = 'S';
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

double SubChunk::getContinentalNoise(vec2 pos, NoiseGenerator &noise_gen)
{
	double noise = 0.0;
	NoiseData nData = {
		1.0, // amplitude
		0.001, // frequency
		0.8, // persistance
		4.0, // lacunarity
		4 // nb_octaves
	};

	noise_gen.setNoiseData(nData);
	noise = noise_gen.noise(pos.x, pos.y);
	noise_gen.setNoiseData(NoiseData());
	return noise;
}

double SubChunk::getMinHeight(vec2 pos, NoiseGenerator &noise_gen)
{
	std::vector<Point> splinePoints = {{-1.0, 50.0}, {0.3, 100.0}, {0.4, 150.0}, {1.0, 150.0}};
	double continentalNoise = getContinentalNoise(pos, noise_gen);
	SplineInterpolator spline(splinePoints);

	double splineResult = spline.interpolate(continentalNoise);
	return splineResult;
}

void SubChunk::loadBiome()
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

SubChunk::~SubChunk()
{
	_loaded = false;
}

char SubChunk::getBlock(vec3 position)
{
	int x = position.x;
	int y = position.y;
	int z = position.z;

	if (x >= CHUNK_SIZE || y >= CHUNK_SIZE || z >= CHUNK_SIZE || x < 0 || y < 0 || z < 0)
		return 0;
	return _blocks[x + (z * CHUNK_SIZE) + (y * CHUNK_SIZE * CHUNK_SIZE)];
}

void SubChunk::addDownFace(vec3 position, TextureType texture)
{
	int x = _position.x * CHUNK_SIZE + position.x;
	int y = _position.y * CHUNK_SIZE + position.y;
	int z = _position.z * CHUNK_SIZE + position.z;

	char block;
	if (position.y > 0)
		block = _blocks[position.x + (position.z * CHUNK_SIZE) + ((position.y - 1) * CHUNK_SIZE * CHUNK_SIZE)];
	else
		block = _world.getBlock(vec3(x, y - 1, z));
	if (block == -1)
		_isFullyLoaded = false;
	else if (block == 0)
		addFace(position, DOWN, texture);
}

void SubChunk::addUpFace(vec3 position, TextureType texture)
{
	int x = _position.x * CHUNK_SIZE + position.x;
	int y = _position.y * CHUNK_SIZE + position.y;
	int z = _position.z * CHUNK_SIZE + position.z;

	char block = -1;
	if (position.y != (CHUNK_SIZE - 1))
		block = _blocks[position.x + (position.z * CHUNK_SIZE) + ((position.y + 1) * CHUNK_SIZE * CHUNK_SIZE)];
	else
		block = _world.getBlock(vec3(x, y + 1, z));
	if (block == -1)
		_isFullyLoaded = false;
	if (block <= 0)
		addFace(position, UP, texture);
}

void SubChunk::addNorthFace(vec3 position, TextureType texture)
{
	int x = _position.x * CHUNK_SIZE + position.x;
	int y = _position.y * CHUNK_SIZE + position.y;
	int z = _position.z * CHUNK_SIZE + position.z;

	char block = -1;
	if (position.z != 0)
		block = _blocks[position.x + ((position.z - 1) * CHUNK_SIZE) + (position.y * CHUNK_SIZE * CHUNK_SIZE)];
	else
		block = _world.getBlock(vec3(x, y, z - 1));
	if (block == -1)
		_isFullyLoaded = false;
	if (block <= 0)
		addFace(position, NORTH, texture);
}

void SubChunk::addSouthFace(vec3 position, TextureType texture)
{
	int x = _position.x * CHUNK_SIZE + position.x;
	int y = _position.y * CHUNK_SIZE + position.y;
	int z = _position.z * CHUNK_SIZE + position.z;

	char block = -1;
	if (position.z != CHUNK_SIZE - 1)
		block = _blocks[position.x + ((position.z + 1) * CHUNK_SIZE) + (position.y * CHUNK_SIZE * CHUNK_SIZE)];
	else
		block = _world.getBlock(vec3(x, y, z + 1));
	if (block == -1)
		_isFullyLoaded = false;
	if (block <= 0)
		addFace(position, SOUTH, texture);
}

void SubChunk::addWestFace(vec3 position, TextureType texture)
{
	int x = _position.x * CHUNK_SIZE + position.x;
	int y = _position.y * CHUNK_SIZE + position.y;
	int z = _position.z * CHUNK_SIZE + position.z;

	char block = -1;
	if (position.x != 0)
		block = _blocks[(position.x - 1) + (position.z * CHUNK_SIZE) + (position.y * CHUNK_SIZE * CHUNK_SIZE)];
	else
		block = _world.getBlock(vec3(x - 1, y, z));
	if (block == -1)
		_isFullyLoaded = false;
	if (block <= 0)
		addFace(position, WEST, texture);
}

void SubChunk::addEastFace(vec3 position, TextureType texture)
{
	int x = _position.x * CHUNK_SIZE + position.x;
	int y = _position.y * CHUNK_SIZE + position.y;
	int z = _position.z * CHUNK_SIZE + position.z;

	char block = -1;
	if (position.x != CHUNK_SIZE - 1)
		block = _blocks[(position.x + 1) + (position.z * CHUNK_SIZE) + (position.y * CHUNK_SIZE * CHUNK_SIZE)];
	else
		block = _world.getBlock(vec3(x + 1, y, z));
	if (block == -1)
		_isFullyLoaded = false;
	if (block <= 0)
		addFace(position, EAST, texture);
}

void SubChunk::addBlock(vec3 position, TextureType down, TextureType up, TextureType north, TextureType south, TextureType east, TextureType west)
{
	addUpFace(position, up);
	addDownFace(position, down);
	addNorthFace(position, north);
	addSouthFace(position, south);
	addWestFace(position, west);
	addEastFace(position, east);
}

vec3 SubChunk::getPosition()
{
	return _position;
}

void SubChunk::clearFaces() {
	for (int i = 0; i < 6; i++)
	{
		_faces[i].clear();
	}
}

void SubChunk::sendFacesToDisplay()
{
	if (_hasSentFaces == true)
	// if (_hasSentFaces == true && _isFullyLoaded == true)
		return ;
	if (_isFullyLoaded == false)
	{
		_vertexData.clear();
		clearFaces();
		_isFullyLoaded = true;
	}
	for (int x = 0; x < CHUNK_SIZE; x++)
	{
		for (int y = 0; y < CHUNK_SIZE; y++)
		{
			for (int z = 0; z < CHUNK_SIZE; z++)
			{
				switch (_blocks[x + (z * CHUNK_SIZE) + (y * CHUNK_SIZE * CHUNK_SIZE)])
				{
					case 'D':
						addBlock(vec3(x, y, z), T_DIRT, T_DIRT, T_DIRT, T_DIRT, T_DIRT, T_DIRT);
						break;
					case 'S':
						addBlock(vec3(x, y, z), T_STONE, T_STONE, T_STONE, T_STONE, T_STONE, T_STONE);
						break;
					case 'G':
						addBlock(vec3(x, y, z), T_DIRT, T_GRASS_TOP, T_GRASS_SIDE, T_GRASS_SIDE, T_GRASS_SIDE, T_GRASS_SIDE);
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
	// if (lengthX == 0)
	// 	lengthX++;
	// if (lengthY == 0)
	// 	lengthY++;

	if (face.direction == EAST || face.direction == WEST)
	{
		lengthX = face.size.y - 1;
		lengthY = face.size.x - 1;
	}
	
	newVertex |= (x & 0x1F) << 0;   // 5 bits for x
	newVertex |= (y & 0x1F) << 5;   // 5 bits for y
	newVertex |= (z & 0x1F) << 10;  // 5 bits for z
	newVertex |= (direction & 0x07) << 15;  // 3 bits for direction
	newVertex |= (lengthX & 0x1F) << 18; // 5 bits for lengthX
	newVertex |= (lengthY & 0x1F) << 23; // 5 bits for lengthY
	newVertex |= (textureID & 0x0F) << 28; // 4 bits for textureID
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
