#include "SubChunk.hpp"

SubChunk::SubChunk(vec3 position, PerlinMap *perlinMap, Chunk &chunk, World &world, TextureManager &textManager, int resolution) : _world(world), _chunk(chunk), _textManager(textManager)
{
	_position = position;
	_resolution = resolution;
	_blocks.resize(CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE);
	std::fill(_blocks.begin(), _blocks.end(), 0);
	_heightMap = &perlinMap->heightMap;
	loadHeight();
	loadBiome();
}

void SubChunk::pushVerticesToOpenGL(bool isTransparent) {
	if (isTransparent) {
		glBindBuffer(GL_ARRAY_BUFFER, _transparentInstanceVBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(int) * _transparentVertexData.size(), _transparentVertexData.data(), GL_STATIC_DRAW);
		_needTransparentUpdate = false;
	} else {
		glBindBuffer(GL_ARRAY_BUFFER, _instanceVBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(int) * _vertexData.size(), _vertexData.data(), GL_STATIC_DRAW);
		_needUpdate = false;
	}
}

void SubChunk::initGLBuffer()
{
	if (_hasBufferInitialized == true)
		return ;
	glGenVertexArrays(1, &_vao);
	glGenVertexArrays(1, &_vbo);
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
	pushVerticesToOpenGL(false);

	glVertexAttribIPointer(2, 1, GL_INT, sizeof(int), (void*)0); // Instance positions
	glEnableVertexAttribArray(2);
	glVertexAttribDivisor(2, 1); // Update once per instance

	glGenVertexArrays(1, &_transparentVao);
	glGenBuffers(1, &_transparentInstanceVBO);

    glBindVertexArray(_transparentVao);
    glBindBuffer(GL_ARRAY_BUFFER, _vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), (void*)0); // Positions
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), (void*)(3 * sizeof(GLfloat))); // Offset
    glEnableVertexAttribArray(1);

    // Instance data (instancePositions)
	pushVerticesToOpenGL(true);

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
	for (int y = 0; y < CHUNK_SIZE ; y += _resolution)
	{
		for (int z = 0; z < CHUNK_SIZE ; z += _resolution)
		{
			for (int x = 0; x < CHUNK_SIZE ; x += _resolution)
			{
				double height = (*_heightMap)[z * CHUNK_SIZE + x];
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
	for (i = 0; i > -4; i--)
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

void SubChunk::loadMountain(int x, int z, size_t ground)
{
	(void)x;
	(void)z;
	(void)ground;
	//
	setBlock(vec3(x, ground - _position.y * CHUNK_SIZE, z), SNOW);
	for (int i = -1; i > -4; i--)
		setBlock(vec3(x, ground + i - _position.y * CHUNK_SIZE, z), SNOW);
	// setBlock(vec3(x, ground - _position.y * CHUNK_SIZE, z), GRASS);
	// for (int i = -1; i > -5; i--)
	// 	setBlock(vec3(x, ground + i - _position.y * CHUNK_SIZE, z), DIRT);
}

void SubChunk::loadBiome()
{
	NoiseGenerator &noisegen = _world.getNoiseGenerator();
	noisegen.setNoiseData({
		1.0,  // amplitude
		0.9, // frequency
		0.02,  // persistence
		0.5,  // lacunarity
		12     // nb_octaves
	});
	for (int x = 0; x < CHUNK_SIZE ; x++)
	{
		for (int z = 0; z < CHUNK_SIZE ; z++)
		{
			double ground = (*_heightMap)[z * CHUNK_SIZE + x];
			if (ground <= OCEAN_HEIGHT)
				loadOcean(x, z, ground + 2);
			else if (ground >= MOUNT_HEIGHT + (noisegen.noise(x + _position.x * CHUNK_SIZE, z + _position.z * CHUNK_SIZE) * 15)) {
				loadMountain(x, z, ground);
			}
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

void SubChunk::addDownFace(BlockType current, vec3 position, TextureType texture, bool isTransparent)
{
	char block = 0;
	if (position.y > 0)
		block = getBlock({position.x, position.y - _resolution, position.z});
	else
	{
		SubChunk *underChunk = nullptr;
		underChunk = _chunk.getSubChunk(_position.y - _resolution);
		if (!underChunk)
			block = '/';
		else
			block = underChunk->getBlock({position.x, CHUNK_SIZE - _resolution, position.z});
	}
	if (faceDisplayCondition(current, block))
		addFace(position, DOWN, texture, isTransparent);
}

void SubChunk::addUpFace(BlockType current, vec3 position, TextureType texture, bool isTransparent)
{
	char block = 0;
	if (position.y != CHUNK_SIZE - _resolution)
		block = getBlock({position.x, position.y + _resolution, position.z});
	else
	{
		SubChunk *overChunk = _chunk.getSubChunk(_position.y + 1);
		if (overChunk)
			block = overChunk->getBlock({position.x, 0, position.z});
	}
	if (faceDisplayCondition(current, block))
		addFace(position, UP, texture, isTransparent);
}

void SubChunk::addNorthFace(BlockType current, vec3 position, TextureType texture, bool isTransparent)
{
	char block = 0;
	if (position.z != 0)
		block = getBlock({position.x, position.y, position.z - _resolution});
	else {
		Chunk *chunk = _chunk.getNorthChunk();
		if (chunk)
		{
			SubChunk *subChunk = chunk->getSubChunk(_position.y);
			if (subChunk) {
				block = subChunk->getBlock(vec3(position.x, position.y, CHUNK_SIZE - _resolution));
			} else {
				block = 1;
			}
		}
	}
	if (faceDisplayCondition(current, block))
		addFace(position, NORTH, texture, isTransparent);
}

void SubChunk::addSouthFace(BlockType current, vec3 position, TextureType texture, bool isTransparent)
{
	char block = 0;
	if (position.z != CHUNK_SIZE - _resolution)
		block = getBlock({position.x, position.y, position.z + _resolution});
	else {
		Chunk *chunk = _chunk.getSouthChunk();
		if (chunk)
		{
			SubChunk *subChunk = chunk->getSubChunk(_position.y);
			if (subChunk) {
				block = subChunk->getBlock(vec3(position.x, position.y, 0));
			} else {
				block = 1;
			}
		}
	}
	if (faceDisplayCondition(current, block))
		addFace(position, SOUTH, texture, isTransparent);
}

void SubChunk::addWestFace(BlockType current, vec3 position, TextureType texture, bool isTransparent)
{
	char block = 0;
	if (position.x != 0)
		block = getBlock({position.x - _resolution, position.y, position.z});
	else {
		Chunk *chunk = _chunk.getWestChunk();
		if (chunk)
		{
			SubChunk *subChunk = chunk->getSubChunk(_position.y);
			if (subChunk) {
				block = subChunk->getBlock(vec3(CHUNK_SIZE - _resolution, position.y, position.z));
			} else {
				block = 1;
			}
		}
	}
	if (faceDisplayCondition(current, block))
		addFace(position, WEST, texture, isTransparent);
}

void SubChunk::addEastFace(BlockType current, vec3 position, TextureType texture, bool isTransparent)
{
	char block = 0;
	if (position.x != CHUNK_SIZE - _resolution)
		block = getBlock({position.x + _resolution, position.y, position.z});
	else {
		Chunk *chunk = _chunk.getEastChunk();
		if (chunk)
		{
			SubChunk *subChunk = chunk->getSubChunk(_position.y);
			if (subChunk) {
				block = subChunk->getBlock(vec3(0, position.y, position.z));
			} else {
				block = 1;
			}
		}
	}
	if (faceDisplayCondition(current, block))
		addFace(position, EAST, texture, isTransparent);
}

void SubChunk::addBlock(BlockType block, vec3 position, TextureType down, TextureType up, TextureType north, TextureType south, TextureType east, TextureType west, bool isTransparent = false)
{
	addUpFace(block, position, up, isTransparent);
	addDownFace(block, position, down, isTransparent);
	addNorthFace(block, position, north, isTransparent);
	addSouthFace(block, position, south, isTransparent);
	addWestFace(block, position, west, isTransparent);
	addEastFace(block, position, east, isTransparent);
}

vec3 SubChunk::getPosition()
{
	return _position;
}

void SubChunk::clearFaces() {
	// std::cout << "Cleared faces" << std::endl;
	for (int i = 0; i < 6; i++)
	{
		_faces[i].clear();
		_transparentFaces[i].clear();
	}
	_vertexData.clear();
	_transparentVertexData.clear();
	_hasSentFaces = false;
	_needUpdate = true;
	_needTransparentUpdate = true;
}

void SubChunk::sendFacesToDisplay()
{
	if (_hasSentFaces == true)
		clearFaces();
	// 	return ;
	for (int x = 0; x < CHUNK_SIZE; x += _resolution)
	{
		for (int y = 0; y < CHUNK_SIZE; y += _resolution)
		{
			for (int z = 0; z < CHUNK_SIZE; z += _resolution)
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
						addBlock(WATER, vec3(x, y, z), T_WATER, T_WATER, T_WATER, T_WATER, T_WATER, T_WATER, true);
						break;
					case SNOW:
						addBlock(SNOW, vec3(x, y, z), T_SNOW, T_SNOW, T_SNOW, T_SNOW, T_SNOW, T_SNOW);
						break;
					default :
						break;
				}
			}
		}
	}
	processFaces(false);
	processFaces(true);
	_hasSentFaces = true;
}

void SubChunk::addTextureVertex(Face face, std::vector<int> *vertexData)
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
	vertexData->push_back(newVertex);
}

int SubChunk::displayTransparent(void)
{
	if (_hasBufferInitialized == false)
		initGLBuffer();
	if (_needTransparentUpdate) {
		pushVerticesToOpenGL(true);
	}
	glBindVertexArray(_transparentVao);
	glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, _transparentVertexData.size());
	glBindVertexArray(0);
	return (_transparentVertexData.size() * 2);
}

int SubChunk::display(void)
{
	if (_hasBufferInitialized == false)
		initGLBuffer();
	if (_needUpdate) {
		pushVerticesToOpenGL(false);
	}
	glBindVertexArray(_vao);
	glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, _vertexData.size());
	glBindVertexArray(0);
	return (_vertexData.size() * 2);
}

void SubChunk::processFaces(bool isTransparent)
{
	// for (int i = 0; i < 6; i++) {
	// 	for (Face face : _faces[i]) {
	// 		addTextureVertex(face);
	// 	}
	// }
	if (isTransparent)
	{
		processUpVertex(_transparentFaces, &_transparentVertexData);
		processDownVertex(_transparentFaces, &_transparentVertexData);
		processNorthVertex(_transparentFaces, &_transparentVertexData);
		processSouthVertex(_transparentFaces, &_transparentVertexData);
		processEastVertex(_transparentFaces, &_transparentVertexData);
		processWestVertex(_transparentFaces, &_transparentVertexData);
	} else {
		processUpVertex(_faces, &_vertexData);
		processDownVertex(_faces, &_vertexData);
		processNorthVertex(_faces, &_vertexData);
		processSouthVertex(_faces, &_vertexData);
		processEastVertex(_faces, &_vertexData);
		processWestVertex(_faces, &_vertexData);
	}
}
