#include "SubChunk.hpp"

SubChunk::SubChunk(ivec3 position, PerlinMap *perlinMap, CaveGenerator &caveGen, Chunk &chunk, World &world, TextureManager &textManager, int resolution)
: _world(world), _chunk(chunk), _textManager(textManager), _caveGen(caveGen)
{
	_position = position;
	_resolution = resolution;
	_heightMap = &perlinMap->heightMap;
	_chunkSize = CHUNK_SIZE / resolution;
	size_t size = _chunkSize * _chunkSize * _chunkSize;
	_blocks = std::make_unique<uint8_t[]>(size);
	std::fill_n(_blocks.get(), size, 0);
	_memorySize = sizeof(*this) + size;
	_isFullyLoaded = false;
}

size_t SubChunk::getMemorySize() {
	return _memorySize;
}

void SubChunk::loadHeight(int prevResolution)
{
	(void)prevResolution;

	for (int z = 0; z < CHUNK_SIZE ; z += _resolution)
	{
		for (int x = 0; x < CHUNK_SIZE ; x += _resolution)
		{
			int maxHeight = (*_heightMap)[z * CHUNK_SIZE + x];
			for (int y = 0; y < CHUNK_SIZE ; y += _resolution)
			{
				int globalY = y + _position.y * CHUNK_SIZE;
				if (globalY <= 0) {
					if (globalY == 0)
						setBlock(x, y, z, BEDROCK);
					continue;
				}
				// Default: solid below surface

				if (globalY > maxHeight)
					break;
	
				if (_resolution != 1 || !_caveGen.isAir(
						x + _position.x * CHUNK_SIZE,
						globalY,
						z + _position.z * CHUNK_SIZE,
						maxHeight + 40
					))
					setBlock(x, y, z, STONE);
			}
		}
	}
}

void SubChunk::loadOcean(int x, int z, size_t ground, size_t adjustOceanHeight)
{
	int y;

	for (y = adjustOceanHeight; y > (int)ground; y--)
		setBlock(x, y - _position.y * CHUNK_SIZE, z, WATER);
	setBlock(x, y - _position.y * CHUNK_SIZE, z, SAND);
	int i;
	for (i = 0; i > -4 * _resolution; i--)
		setBlock(x, y + i - _position.y * CHUNK_SIZE, z, SAND);
	for (; i > -8 * _resolution; i--)
		setBlock(x, y + i - _position.y * CHUNK_SIZE, z, DIRT);
}

void SubChunk::loadPlaine(int x, int z, size_t ground)
{
	int y = ground - _position.y * CHUNK_SIZE;

	if (getBlock({x, y, z}) == STONE)
		setBlock(x, y, z, GRASS);
	for (int i = 1; i <= 4; i++)
		if (getBlock({x, y - (i * _resolution), z}) == STONE)
			setBlock(x, y - (i * _resolution), z, DIRT);
}
void SubChunk::loadMountain(int x, int z, size_t ground)
{
	int y = ground - _position.y * CHUNK_SIZE;

	if (getBlock({x, y, z}) == STONE)
		setBlock(x, y, z, SNOW);
	for (int i = 1; i <= 4; i++)
		if (getBlock({x, y - (i * _resolution), z}) == STONE)
			setBlock(x, y - (i * _resolution), z, SNOW);
}

void SubChunk::loadBiome(int prevResolution)
{
	(void)prevResolution
;	NoiseGenerator &noisegen = _world.getNoiseGenerator();
	noisegen.setNoiseData({
		1.0, 0.9, 0.02, 0.5, 12
	});

	const int adjustOceanHeight = OCEAN_HEIGHT - (OCEAN_HEIGHT % _resolution);

	for (int x = 0; x < CHUNK_SIZE ; x += _resolution)
	{
		for (int z = 0; z < CHUNK_SIZE ; z += _resolution)
		{
			double ground = (*_heightMap)[z * CHUNK_SIZE + x];
			ground = ground - (int(ground) % _resolution);
		
			if (ground <= OCEAN_HEIGHT)
				loadOcean(x, z, ground + 3, adjustOceanHeight);
			else if (ground >= MOUNT_HEIGHT + (noisegen.noise(x + _position.x * CHUNK_SIZE, z + _position.z * CHUNK_SIZE) * 15))
				loadMountain(x, z, ground);
			else if (ground)
				loadPlaine(x, z, ground);
		}
	}
	_isFullyLoaded = true;
}

SubChunk::~SubChunk()
{
	_loaded = false;
}

void SubChunk::setBlock(int x, int y, int z, char block)
{
	x /= _resolution;
	y /= _resolution;
	z /= _resolution;
	if (_chunkSize <= 0) return;
	if (x >= _chunkSize || y >= _chunkSize || z >= _chunkSize || x < 0 || y < 0 || z < 0)
		return ;
	_blocks[x + (z * _chunkSize) + (y * _chunkSize * _chunkSize)] = block;
}

char SubChunk::getBlock(ivec3 position)
{
	int x = position.x;
	int y = position.y;
	int z = position.z;

	x /= _resolution;
	y /= _resolution;
	z /= _resolution;
	if (x >= _chunkSize || y >= _chunkSize || z >= _chunkSize || x < 0 || y < 0 || z < 0)
		return 0;
	return _blocks[x + (z * _chunkSize) + (y * _chunkSize * _chunkSize)];
}

void SubChunk::addDownFace(BlockType current, ivec3 position, TextureType texture, bool isTransparent)
{
	char block = 0;
	if (position.y > 0)
		block = getBlock({position.x, position.y - _resolution, position.z});
	else
	{
		SubChunk *underChunk = nullptr;
		underChunk = _chunk.getSubChunk(_position.y - 1);
		if (!underChunk)
			block = AIR;
		else
			block = underChunk->getBlock({position.x, CHUNK_SIZE - _resolution, position.z});
	}
	if (faceDisplayCondition(current, block))
		addFace(position, DOWN, texture, isTransparent);
}

void SubChunk::addUpFace(BlockType current, ivec3 position, TextureType texture, bool isTransparent)
{
	char block = 0;
	if (position.y != CHUNK_SIZE - _resolution)
		block = getBlock({position.x, position.y + _resolution, position.z});
	else
	{
		SubChunk *overChunk = _chunk.getSubChunk(_position.y + 1);
		if (overChunk)
			block = overChunk->getBlock({position.x, 0, position.z});
		else
			block = AIR;

	}
	if (faceDisplayCondition(current, block))
		addFace(position, UP, texture, isTransparent);
}

void SubChunk::addNorthFace(BlockType current, ivec3 position, TextureType texture, bool isTransparent)
{
	if (position.z != 0)
	{
		if (isNeighborTransparent(ivec3(position.x, position.y, position.z - _resolution), NORTH, current, _resolution))
			return addFace(position, NORTH, texture, isTransparent);
		return ;
	}
	Chunk *chunk = _chunk.getNorthChunk();
	if (!chunk)
		return ;

	SubChunk *subChunk = chunk->getSubChunk(_position.y);
	if (!subChunk || !subChunk->isNeighborTransparent(ivec3(position.x, position.y, CHUNK_SIZE - subChunk->_resolution), NORTH, current, _resolution))
		return ;

	addFace(position, NORTH, texture, isTransparent);
}

void SubChunk::addSouthFace(BlockType current, ivec3 position, TextureType texture, bool isTransparent)
{
	if (position.z != CHUNK_SIZE - _resolution)
	{
		if (isNeighborTransparent(ivec3(position.x, position.y, position.z + _resolution), SOUTH, current, _resolution))
			return addFace(position, SOUTH, texture, isTransparent);
		return ;
	}
	
	Chunk *chunk = _chunk.getSouthChunk();
	if (!chunk)
		return ;

	SubChunk *subChunk = chunk->getSubChunk(_position.y);
	if (!subChunk || !subChunk->isNeighborTransparent(ivec3(position.x, position.y, 0), SOUTH, current, _resolution))
		return ;

	addFace(position, SOUTH, texture, isTransparent);
}

void SubChunk::addWestFace(BlockType current, ivec3 position, TextureType texture, bool isTransparent)
{
	if (position.x != 0)
	{
		if (isNeighborTransparent(ivec3(position.x - _resolution, position.y, position.z), WEST, current, _resolution))
			return addFace(position, WEST, texture, isTransparent);
		return ;
	}
	
	Chunk *chunk = _chunk.getWestChunk();
	if (!chunk)
		return ;

	SubChunk *subChunk = chunk->getSubChunk(_position.y);
	if (!subChunk || !subChunk->isNeighborTransparent(ivec3(CHUNK_SIZE - subChunk->_resolution, position.y, position.z), WEST, current, _resolution))
		return ;

	addFace(position, WEST, texture, isTransparent);
}

void SubChunk::addEastFace(BlockType current, ivec3 position, TextureType texture, bool isTransparent)
{
	if (position.x != CHUNK_SIZE - _resolution)
	{
		if (isNeighborTransparent(ivec3(position.x + _resolution, position.y, position.z), EAST, current, _resolution))
			return addFace(position, EAST, texture, isTransparent);
		return ;
	}
	
	Chunk *chunk = _chunk.getEastChunk();
	if (!chunk)
		return ;

	SubChunk *subChunk = chunk->getSubChunk(_position.y);
	if (!subChunk || !subChunk->isNeighborTransparent(ivec3(0, position.y, position.z), EAST, current, _resolution))
		return ;

	addFace(position, EAST, texture, isTransparent);
}

void SubChunk::addBlock(BlockType block, ivec3 position, TextureType down, TextureType up, TextureType north, TextureType south, TextureType east, TextureType west, bool isTransparent = false)
{
	addUpFace(block, position, up, isTransparent);
	addDownFace(block, position, down, isTransparent);
	addNorthFace(block, position, north, isTransparent);
	addSouthFace(block, position, south, isTransparent);
	addWestFace(block, position, west, isTransparent);
	addEastFace(block, position, east, isTransparent);
}

// Show all faces
// void SubChunk::addBlock(BlockType block, ivec3 position, TextureType down, TextureType up, TextureType north, TextureType south, TextureType east, TextureType west, bool isTransparent = false)
// {
// 	(void)block;
// 	addFace(position, NORTH, north, isTransparent);
// 	addFace(position, SOUTH, south, isTransparent);
// 	addFace(position, WEST, west, isTransparent);
// 	addFace(position, UP, up, isTransparent);
// 	addFace(position, DOWN, down, isTransparent);
// 	addFace(position, EAST, east, isTransparent);
// }

ivec3 SubChunk::getPosition()
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

void SubChunk::updateResolution(int resolution, PerlinMap *perlinMap)
{
	int prevResolution = _resolution;
	_resolution = resolution;
	_heightMap = &perlinMap->heightMap;

	_chunkSize = CHUNK_SIZE / resolution;
	size_t size = _chunkSize * _chunkSize * _chunkSize;
	_blocks = std::make_unique<uint8_t[]>(size);
	loadHeight(prevResolution);
	loadBiome(prevResolution);
}


void SubChunk::sendFacesToDisplay()
{
	if (!_isFullyLoaded)
		return ;
	clearFaces();

	for (int x = 0; x < CHUNK_SIZE; x += _resolution)
	{
		for (int y = 0; y < CHUNK_SIZE; y += _resolution)
		{
			for (int z = 0; z < CHUNK_SIZE; z += _resolution)
			{
				switch (getBlock({x, y, z}))
				{
					case 0:
						break;
					case DIRT:
						addBlock(DIRT, ivec3(x, y, z), T_DIRT, T_DIRT, T_DIRT, T_DIRT, T_DIRT, T_DIRT);
						break;
					case COBBLE:
						addBlock(COBBLE, ivec3(x, y, z), T_COBBLE, T_COBBLE, T_COBBLE, T_COBBLE, T_COBBLE, T_COBBLE);
						break;
					case BEDROCK:
						addBlock(BEDROCK, ivec3(x, y, z), T_BEDROCK, T_BEDROCK, T_BEDROCK, T_BEDROCK, T_BEDROCK, T_BEDROCK);
						break;
					case STONE:
						addBlock(STONE, ivec3(x, y, z), T_STONE, T_STONE, T_STONE, T_STONE, T_STONE, T_STONE);
						break;
					case GRASS:
						addBlock(GRASS, ivec3(x, y, z), T_DIRT, T_GRASS_TOP, T_GRASS_SIDE, T_GRASS_SIDE, T_GRASS_SIDE, T_GRASS_SIDE);
						break;
					case SAND:
						addBlock(SAND, ivec3(x, y, z), T_SAND, T_SAND, T_SAND, T_SAND, T_SAND, T_SAND);
						break;
					case WATER:
						addBlock(WATER, ivec3(x, y, z), T_WATER, T_WATER, T_WATER, T_WATER, T_WATER, T_WATER, true);
						break;
					case SNOW:
						addBlock(SNOW, ivec3(x, y, z), T_SNOW, T_SNOW, T_SNOW, T_SNOW, T_SNOW, T_SNOW);
						break;
					default :
						break;
				}
			}
		}
	}
	processFaces(false);
	processFaces(true);
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

void SubChunk::processFaces(bool isTransparent)
{
	if (isTransparent)
	{
		processUpVertex(_transparentFaces, &_transparentVertexData);
		processDownVertex(_transparentFaces, &_transparentVertexData);
		processNorthVertex(_transparentFaces, &_transparentVertexData);
		processSouthVertex(_transparentFaces, &_transparentVertexData);
		processEastVertex(_transparentFaces, &_transparentVertexData);
		processWestVertex(_transparentFaces, &_transparentVertexData);
	}
	else
	{
		processUpVertex(_faces, &_vertexData);
		processDownVertex(_faces, &_vertexData);
		processNorthVertex(_faces, &_vertexData);
		processSouthVertex(_faces, &_vertexData);
		processEastVertex(_faces, &_vertexData);
		processWestVertex(_faces, &_vertexData);
	}
}

std::vector<int> &SubChunk::getVertices() {
	return _vertexData;
}

std::vector<int> &SubChunk::getTransparentVertices()
{
	return _transparentVertexData;
}

# define IS_TRANSPARENT true
# define IS_SOLID false

bool SubChunk::isNeighborTransparent(ivec3 position, Direction dir, char viewerBlock, int viewerResolution) {
	if (viewerResolution == _resolution)
		return (faceDisplayCondition(viewerBlock, getBlock(position)));
	if (viewerResolution < _resolution)
		return (IS_SOLID);
	position /= _resolution;
	position *= _resolution;
	int res2 = _resolution * 2;
	for (int x = 0; x < res2; x += _resolution) {
		for (int y = 0; y < res2; y += _resolution) {
			for (int z = 0; z < res2; z += _resolution) {
				if (faceDisplayCondition(viewerBlock, getBlock(ivec3(position.x + x, position.y + y, position.z + z))))
					return IS_TRANSPARENT;
				if (dir == NORTH || dir == SOUTH)
					break ;
				}
				if (dir == UP || dir == DOWN)
					break ;
			}
		if (dir == EAST || dir == WEST)
			break ;
	}
	return IS_SOLID;
}
