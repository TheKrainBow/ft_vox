#include "Chunk.hpp"

Chunk::Chunk(ivec2 pos, PerlinMap *perlinMap, World &world, TextureManager &textureManager) : _world(world), _textureManager(textureManager)
{
	_isInit = false;
	_perlinMap = perlinMap;
	_position = pos;
	_facesSent = false;
	_needUpdate = true;
	_hasBufferInitialized = false;
	getNeighbors();
	int heighest = perlinMap->heighest;
	int lowest = perlinMap->lowest;
	if (heighest < OCEAN_HEIGHT) {
		heighest = OCEAN_HEIGHT;
	}
	heighest = heighest / CHUNK_SIZE * CHUNK_SIZE;
	lowest = lowest / CHUNK_SIZE * CHUNK_SIZE;
	for (int y = (lowest) - (CHUNK_SIZE); y < (heighest) + (CHUNK_SIZE * 2); y += CHUNK_SIZE)
	{
		_subChunks[y / CHUNK_SIZE] = new SubChunk({pos.x, int(y / CHUNK_SIZE), pos.y}, perlinMap, *this, world, textureManager);
	}
	_isInit = true;
	sendFacesToDisplay();
	if (_north)
		_north->sendFacesToDisplay();
	if (_south)
		_south->sendFacesToDisplay();
	if (_east)
		_east->sendFacesToDisplay();
	if (_west)
		_west->sendFacesToDisplay();
}

Chunk::~Chunk()
{
	_subChunksMutex.lock();
	for (auto &subchunk : _subChunks)
		delete subchunk.second;
	_subChunksMutex.unlock();
	_subChunks.clear();
}

void Chunk::getNeighbors()
{
	// if (_isFullyLoaded)
	// 	return ;
	// _chrono.startChrono(2, "Getting chunks");
    // std::future<Chunk *> retNorth = std::async(std::launch::async, [this]() {
    //     return _world.getChunk({_position.x + 1, _position.y});
    // });

    // std::future<Chunk *> retSouth = std::async(std::launch::async, [this]() {
    //     return _world.getChunk({_position.x - 1, _position.y});
    // });

    // std::future<Chunk *> retEast = std::async(std::launch::async, [this]() {
    //     return _world.getChunk({_position.x, _position.y + 1});
    // });

    // std::future<Chunk *> retWest = std::async(std::launch::async, [this]() {
    //     return _world.getChunk({_position.x, _position.y - 1});
    // });

    // Wait for all futures and assign the results
    _north = _world.getChunk({_position.x, _position.y - 1});
    _south = _world.getChunk({_position.x, _position.y + 1});
    _east = _world.getChunk({_position.x + 1, _position.y});
    _west = _world.getChunk({_position.x - 1, _position.y});
    if (_north) {
		_north->setSouthChunk(this);
	}

    // _south = retSouth.get();
    if (_south) {
		_south->setNorthChunk(this);
	}

    // _east = retEast.get();
    if (_east) {
		_east->setWestChunk(this);
	}

    // _west = retWest.get();
    if (_west) {
		_west->setEastChunk(this);
	}

	// _chrono.stopChrono(2);
	// _chrono.startChrono(3, "Sending faces");
	// _chrono.stopChrono(3);
	// _chrono.printChronos();
}


ivec2 Chunk::getPosition()
{
	return _position;
}

// int Chunk::displayTransparent()
// {
// 	if (!_facesSent)
// 		return (0);
// 	int triangleDrawn = 0;
// 	for (auto &subchunk : _subChunks)
// 	{
// 		_subChunksMutex.lock();
// 		triangleDrawn += subchunk.second->displayTransparent();
// 		_subChunksMutex.unlock();
// 	}
// 	return triangleDrawn;
// }

void Chunk::pushVerticesToOpenGL(bool isTransparent) {
	// if (isTransparent) {
	// 	glBindBuffer(GL_ARRAY_BUFFER, _transparentInstanceVBO);
	// 	glBufferData(GL_ARRAY_BUFFER, sizeof(int) * _transparentVertexData.size(), _transparentVertexData.data(), GL_STATIC_DRAW);
	// 	_needTransparentUpdate = false;
	// } else {
	// }
	(void)isTransparent;

	glBindBuffer(GL_DRAW_INDIRECT_BUFFER, _indirectBuffer);
	glBufferData(GL_DRAW_INDIRECT_BUFFER, sizeof(DrawArraysIndirectCommand) * _indirectBufferData.size(), _indirectBufferData.data(), GL_STATIC_DRAW);
	glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);

	glNamedBufferStorage(_ssbo, 
		sizeof(glm::vec4) * _indirectBufferData.size(), 
		(const void *)_ssboData.data(), 
		GL_DYNAMIC_STORAGE_BIT);
	
	glBindBuffer(GL_ARRAY_BUFFER, _instanceVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(int) * _vertexData.size(), _vertexData.data(), GL_STATIC_DRAW);
	_needUpdate = false;
}

void Chunk::clearFaces() {
	_vertexData.clear();
	_ssboData.clear();
	_indirectBufferData.clear();
	// _transparentVertexData.clear();
	// _hasSentFaces = false;
	_needUpdate = true;
	// _needTransparentUpdate = true;
}

void Chunk::initGLBuffer()
{
	if (_hasBufferInitialized == true)
		return ;
	glGenVertexArrays(1, &_vao);
	glGenBuffers(1, &_vbo);
	glGenBuffers(1, &_instanceVBO);
	glGenBuffers(1, &_indirectBuffer);
	glCreateBuffers(1, &_ssbo);

    GLfloat vertices[] = {
        0, 0, 0, float(_position.x) * CHUNK_SIZE, 0, float(_position.y) * CHUNK_SIZE,
        1, 0, 0, float(_position.x) * CHUNK_SIZE, 0, float(_position.y) * CHUNK_SIZE,
        0, 1, 0, float(_position.x) * CHUNK_SIZE, 0, float(_position.y) * CHUNK_SIZE,
        1, 1, 0, float(_position.x) * CHUNK_SIZE, 0, float(_position.y) * CHUNK_SIZE,
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


	// glGenVertexArrays(1, &_transparentVao);
	// glGenBuffers(1, &_transparentInstanceVBO);

    // glBindVertexArray(_transparentVao);
    // glBindBuffer(GL_ARRAY_BUFFER, _vbo);
    // glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    // glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), (void*)0); // Positions
    // glEnableVertexAttribArray(0);
    // glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), (void*)(3 * sizeof(GLfloat))); // Offset
    // glEnableVertexAttribArray(1);

    // // Instance data (instancePositions)
	// pushVerticesToOpenGL(true);

	// glVertexAttribIPointer(2, 1, GL_INT, sizeof(int), (void*)0); // Instance positions
	// glEnableVertexAttribArray(2);
	// glVertexAttribDivisor(2, 1); // Update once per instance

    glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
	_hasBufferInitialized = true;
}

SubChunk *Chunk::getSubChunk(int y)
{
	if (_isInit == false)
		return nullptr;
	auto it = _subChunks.find(y);
	auto endIt = _subChunks.end();
	if (it != endIt)
		return it->second;
	return nullptr;
}

bool Chunk::isReady()
{
	return _facesSent;
}

int Chunk::display(void)
{
	_subChunksMutex.lock();
	if (_hasBufferInitialized == false)
		initGLBuffer();
	if (_needUpdate) {
		pushVerticesToOpenGL(false);
	}
	long long size = _vertexData.size();

	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, _ssbo);
	glBindVertexArray(_vao);
	glBindBuffer(GL_DRAW_INDIRECT_BUFFER, _indirectBuffer);

	glMultiDrawArraysIndirect(GL_TRIANGLE_STRIP, nullptr, _indirectBufferData.size(), 0);

	glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
	glBindVertexArray(0);
	_subChunksMutex.unlock();
	return (size * 2);
}

void Chunk::sendFacesToDisplay()
{
	_subChunksMutex.lock();
	if (_facesSent == true)
		clearFaces();
	for (auto &subChunk : _subChunks)
	{
		subChunk.second->sendFacesToDisplay();
		std::vector<int> vertices = subChunk.second->getVertices();
		_indirectBufferData.push_back(DrawArraysIndirectCommand{
			4,
			uint(vertices.size()),
			0,
			uint(_vertexData.size()),
		});
		vec3 pos = subChunk.second->getPosition();
		_ssboData.push_back(glm::vec4{
			pos.x * CHUNK_SIZE, pos.y * CHUNK_SIZE, pos.z * CHUNK_SIZE, 0
		});
		_vertexData.insert(_vertexData.end(), vertices.begin(), vertices.end());
	}
	_facesSent = true;
	_subChunksMutex.unlock();
}

void Chunk::setNorthChunk(Chunk *chunk)
{
	_north = chunk;
}

void Chunk::setSouthChunk(Chunk *chunk)
{
	_south = chunk;
}

void Chunk::setEastChunk(Chunk *chunk)
{
	_east = chunk;
}

void Chunk::setWestChunk(Chunk *chunk)
{
	_west = chunk;
}

Chunk *Chunk::getNorthChunk() {
	return _north;
}

Chunk *Chunk::getSouthChunk() {
	return _south;
}

Chunk *Chunk::getEastChunk() {
	return _east;
}

Chunk *Chunk::getWestChunk() {
	return _west;
}


// Load chunks: 0,203s
// Load chunks: 0,256s
// Load chunks: 0,245s
// Load chunks: 0,235s
// Load chunks: 0,208s