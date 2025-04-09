#include "World.hpp"
#include "define.hpp"
#include <cmath>
#include <iostream>
#include <memory>
#include <map>
#include <tuple>
#include <iostream>
#include <future>
#include <thread>

// Helper function to calculate block position within a chunk
ivec3 World::calculateBlockPos(ivec3 position) const
{
	auto mod = [](int value) { return (value >= 0) ? value % CHUNK_SIZE : (CHUNK_SIZE + (value % CHUNK_SIZE)) % CHUNK_SIZE; };
	return { mod(position.x), mod(position.y), mod(position.z) };
}

World::World(int seed, TextureManager &textureManager, Camera &camera) : _textureManager(textureManager), _camera(&camera), _threadPool(8), _perlinGenerator(seed)
{
	_needUpdate = true;
	_hasBufferInitialized = false;
	_renderDistance = RENDER_DISTANCE;
	_drawnSSBOSize = 1;
}

void World::init(GLuint shaderProgram, int renderDistance = RENDER_DISTANCE) {
	_shaderProgram = shaderProgram;
	_renderDistance = renderDistance;
}

World::~World()
{
	std::lock_guard<std::mutex> lock(_chunksMutex);
	for (auto it = _chunks.begin(); it != _chunks.end(); it++)
		delete it->second;
}

NoiseGenerator &World::getNoiseGenerator(void)
{
	return (_perlinGenerator);
}

void World::loadPerlinMap(vec3 camPosition)
{
	_perlinGenerator.clearPerlinMaps();
	ivec2 position;
	for (int x = -RENDER_DISTANCE; x < RENDER_DISTANCE; x++)
	{
		for (int z = -RENDER_DISTANCE; z < RENDER_DISTANCE; z++)
		{
			position.x = trunc(camPosition.x / CHUNK_SIZE) + x;
			position.y = trunc(camPosition.z / CHUNK_SIZE) + z;
			_perlinGenerator.addPerlinMap(position, CHUNK_SIZE, 1);
		}
	}
}

int *World::getRenderDistancePtr()
{
	return &_renderDistance;
}

void World::setRunning(std::mutex *runningMutex, bool *isRunning)
{
	_isRunning = isRunning;
	_runningMutex = runningMutex;
}

bool World::getIsRunning()
{
	std::lock_guard<std::mutex> lockGuard(*_runningMutex);
	return *_isRunning;
}

void World::unloadChunk()
{
	//TODO Save or do not unload modified chunks (delete block)
	//(Add a isModified boolean in Chunk or SubChunk class)
	//TODO Protect from display segv when cache_size is smaller than double the surface
	// (defines to change to reproduce: CACHE_SIZE 2500 RENDER_DISTANCE 61)
	_chunksListMutex.lock();
	if (_chunkList.size() < CACHE_SIZE)
	{
		_chunksListMutex.unlock();
		return;
	}

	// Get player position in chunk coordinates
	vec3 playerPos = _camera->getWorldPosition();
	int playerChunkX = playerPos.x / CHUNK_SIZE;
	int playerChunkZ = playerPos.z / CHUNK_SIZE;
	if (playerPos.x < 0) playerChunkX--;
	if (playerPos.z < 0) playerChunkZ--;

	// Find the farthest chunk
	auto farthestChunkIt = _chunkList.end();
	float maxDistance = -1.0f;
	for (auto it = _chunkList.begin(); it != _chunkList.end(); ++it)
	{
		Chunk* chunk = *it;
		int chunkX = chunk->getPosition().x;
		int chunkZ = chunk->getPosition().y;
		float distance = sqrt(pow(chunkX - playerChunkX, 2) + pow(chunkZ - playerChunkZ, 2));
		if (distance > maxDistance)
		{
			maxDistance = distance;
			farthestChunkIt = it;
		}
	}

	if (farthestChunkIt != _chunkList.end())
	{
		Chunk* chunkToRemove = *farthestChunkIt;
		// Remove from _chunkList
		_chunkList.erase(farthestChunkIt);
		_chunksListMutex.unlock();

		ivec2 pos = chunkToRemove->getPosition();

		// Remove from _chunks
		_chunksMutex.lock();
		_chunks.erase(pos);
		_chunksMutex.unlock();

		// Clean up memory
		_perlinGenerator.removePerlinMap(pos.x, pos.y);
		chunkToRemove->freeSubChunks();
		delete chunkToRemove;
	}
	else
	{
		_chunksListMutex.unlock();
	}
}

void World::loadChunk(int x, int z, int render, ivec2 chunkPos, int resolution, Direction dir)
{
	Chunk *chunk = nullptr;
	ivec2 pos = {chunkPos.x - render / 2 + x, chunkPos.y - render / 2 + z};
	_chunksMutex.lock();
	auto it = _chunks.find(pos);
	auto itend = _chunks.end();
	if (it != itend)
	{
		chunk = it->second;
		_chunksMutex.unlock();
		if (chunk->_resolution != resolution)
			chunk->updateResolution(resolution, dir);
	}
	else
	{
		_chunksMutex.unlock();
		chunk = new Chunk(pos, _perlinGenerator.getPerlinMap(pos, resolution), *this, _textureManager, resolution);

		_chunksListMutex.lock();
		_chunkList.emplace_back(chunk);
		_chunksListMutex.unlock();

		_chunksMutex.lock();
		_chunks[pos] = chunk;
		chunk->getNeighbors();
		_chunksMutex.unlock();
	}
	_chunksLoadLoadMutex.lock();
	_chunksLoadLoadOrder.emplace(chunk);
	_displayedChunks[pos] = chunk;
	_chunksLoadLoadMutex.unlock();
	// unloadChunk();
}

void World::loadTopChunks(int render, ivec2 chunkPos, int resolution)
{
	int z = 0;
	for (int x = 0; x < render && getIsRunning(); x++)
	{
		loadChunk(x, z, render, chunkPos, resolution, NORTH);
	}
}

void World::loadBotChunks(int render, ivec2 chunkPos, int resolution)
{
	int z = render - 1;
	for (int x = render - 1; getIsRunning() && x >= 0; x--)
	{
		loadChunk(x, z, render, chunkPos, resolution, SOUTH);
	}
}

void World::loadRightChunks(int render, ivec2 chunkPos, int resolution)
{
	int x = render - 1;
	for (int z = 0; z < render && getIsRunning(); z++)
	{
		loadChunk(x, z, render, chunkPos, resolution, EAST);
	}
}

void World::loadLeftChunks(int render, ivec2 chunkPos, int resolution)
{
	int x = 0;
	for (int z = render - 1; getIsRunning() && z >= 0; z--)
	{
		loadChunk(x, z, render, chunkPos, resolution, WEST);
	}
}

void World::loadFirstChunks(ivec2 chunkPos)
{
	int renderDistance = _renderDistance;
	_skipLoad = false;


	int resolution = RESOLUTION;
	_threshold = 32;
	std::vector<std::future<void>> retLst;
	// loadChunk(0, 0, 1, chunkPos);
    for (int render = 0; getIsRunning() && render < renderDistance; render += 2)
	{
		// Load chunks
		std::future<void> retTop = _threadPool.enqueue(&World::loadTopChunks, this, render, chunkPos, resolution);
		std::future<void> retRight = _threadPool.enqueue(&World::loadRightChunks, this, render, chunkPos, resolution);
		std::future<void> retBot = _threadPool.enqueue(&World::loadBotChunks, this, render, chunkPos, resolution);
		std::future<void> retLeft = _threadPool.enqueue(&World::loadLeftChunks, this, render, chunkPos, resolution);
		retTop.get();
		retRight.get();
		retBot.get();
		retLeft.get();

		if (render >= _threshold)
		{
			resolution *= 2;
			_threshold = _threshold * 2;
		}
		// _chunksLoadMutex.lock();
		while (!_chunksLoadLoadOrder.empty())
		{
			_needUpdate = true;
			Chunk *chunk = _chunksLoadLoadOrder.front();
			_chunksLoadMutex.lock();
			_chunksLoadOrder.emplace(chunk);
			_chunksLoadMutex.unlock();
			_chunksLoadLoadOrder.pop();
		}
		// _chunksLoadMutex.unlock();

		if (hasMoved(chunkPos))
			break;
    }

	// for (std::future<void> &ret : retLst)
	// {
	// 	ret.get();
	// }
	// retLst.clear();
}

void World::unLoadNextChunks(ivec2 newCamChunk)
{
	ivec2 pos;
	std::queue<ivec2> deleteQueue;
	for (auto &it : _displayedChunks)
	{
		Chunk *chunk = it.second;
		ivec2 chunkPos = chunk->getPosition();
		if (abs((int)chunkPos.x - (int)newCamChunk.x) > _renderDistance / 2
		|| abs((int)chunkPos.y - (int)newCamChunk.y) > _renderDistance / 2)
		{
			deleteQueue.emplace(chunkPos);
		}
	}
	// std::cout << deleteQueue.size() << std::endl;
	while (!deleteQueue.empty())
	{
		pos = deleteQueue.front();
		_chunksRemovalMutex.lock();
		_chunkRemovalOrder.emplace(pos);
		_chunksRemovalMutex.unlock();
		_displayedChunks.erase(pos);
		deleteQueue.pop();
	}
}

bool World::hasMoved(ivec2 oldPos)
{
	ivec2 camChunk = _camera->getChunkPosition(CHUNK_SIZE);

	if (((floor(oldPos.x) != floor(camChunk.x) || floor(oldPos.y) != floor(camChunk.y))))
		return true;
	return false;
}

char World::getBlock(ivec3 position)
{
	// std::cout << "World::getBlock" << std::endl;
	ivec3 chunkPos(position.x / CHUNK_SIZE, position.y / CHUNK_SIZE, position.z / CHUNK_SIZE);
	chunkPos.x -= (position.x < 0 && abs(position.x) % CHUNK_SIZE != 0);
	chunkPos.z -= (position.z < 0 && abs(position.z) % CHUNK_SIZE != 0);
	chunkPos.y -= (position.y < 0 && abs(position.y) % CHUNK_SIZE != 0);

	Chunk *chunk = getChunk(ivec2(chunkPos.x, chunkPos.z));
	if (!chunk)
	{
		// std::cout << "No chunk (" << chunkPos.x << ", " << chunkPos.z << ")" << std::endl;
		return 0;
	}
	SubChunk *subChunk = chunk->getSubChunk(chunkPos.y);
	// SubChunk *chunk = getSubChunk(chunkPos);
	if (!subChunk)
	{
		// std::cout << "No subchunk (" << chunkPos.y << ")" << std::endl;
		//std::cout << "Couldn't get Subchunk in: " << chunkPos.x << " " << chunkPos.y << " " << chunkPos.z << std::endl;
		return 0;
	}
	return subChunk->getBlock(calculateBlockPos(position));
}

SubChunk *World::getSubChunk(ivec3 position)
{
	_chunksMutex.lock();
	auto it = _chunks.find(ivec2(position.x, position.z));
	auto itend = _chunks.end();
	_chunksMutex.unlock();
	if (it != itend)
		return it->second->getSubChunk(position.y);
	return nullptr;
}

Chunk *World::getChunk(ivec2 position)
{
	// _chunksMutex.lock();
	auto it = _chunks.find({position.x, position.y});
	auto itend = _chunks.end();
	// _chunksMutex.unlock();
	if (it != itend)
		return it->second;
	return nullptr;
}

void World::loadOrder()
{
	_chunksLoadMutex.lock();
	while (!_chunksLoadOrder.empty())
	{
		_needUpdate = true;
		Chunk *chunk = nullptr;
		chunk = _chunksLoadOrder.front();
		ivec2 chunkPos = chunk->getPosition();
		_activeChunks[{chunkPos.x, chunkPos.y}] = chunk;
		_chunksLoadOrder.pop();
	}
	_chunksLoadMutex.unlock();
}

void World::removeOrder()
{
	_chunksRemovalMutex.lock();
	while (!_chunkRemovalOrder.empty())
	{
		_needUpdate = true;
		ivec2 pos;
		pos = _chunkRemovalOrder.front();
		_activeChunks.erase(pos);
		_chunkRemovalOrder.pop();
	}
	_chunksRemovalMutex.unlock();
}

void World::updateActiveChunks()
{
	loadOrder();
	removeOrder();
	if (_needUpdate)
		sendFacesToDisplay();
}

void World::sendFacesToDisplay()
{
	clearFaces();
	for (auto &chunk : _activeChunks)
	{
		size_t size = _vertexData.size();
		std::vector<int> vertices = chunk.second->getVertices();
		_vertexData.insert(_vertexData.end(), vertices.begin(), vertices.end());
		std::vector<DrawArraysIndirectCommand> indirectBufferData = chunk.second->getIndirectData();
		for (DrawArraysIndirectCommand &tmp : indirectBufferData) {
			tmp.baseInstance += size;
		}
		_indirectBufferData.insert(_indirectBufferData.end(), indirectBufferData.begin(), indirectBufferData.end());
		std::vector<vec4> ssboData = chunk.second->getSSBO();
		_ssboData.insert(_ssboData.end(), ssboData.begin(), ssboData.end());
	}
	bool needUpdate = false;
	size_t size = _ssboData.size() * 2;
	while (size > _drawnSSBOSize) {
		_drawnSSBOSize *= 2;
		needUpdate = true;
	}
	if (needUpdate) {
		glDeleteBuffers(1, &_ssbo);
		glCreateBuffers(1, &_ssbo);
		glNamedBufferStorage(_ssbo, 
			sizeof(ivec4) * _drawnSSBOSize, 
			nullptr, 
			GL_DYNAMIC_STORAGE_BIT);
		glNamedBufferSubData(
			_ssbo,
			0,
			sizeof(ivec4) * _ssboData.size(),
			_ssboData.data()
		);
	}
}

int World::displayTransparent()
{
	int trianglesDrawn = 0;

	// glDisable(GL_CULL_FACE);
	// for (auto &activeChunk : _activeChunks)
	// {
	// 	trianglesDrawn += activeChunk.second->displayTransparent();
	// }
	return trianglesDrawn;
}

int World::display()
{
	glEnable(GL_CULL_FACE);
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
	return (size * 2);
	// int trianglesDrawn = 0;
	// for (auto &activeChunk : _activeChunks)
	// {
	// 	trianglesDrawn += activeChunk.second->display();
	// }
	// return trianglesDrawn;
}

int	World::getCachedChunksNumber()
{
	return _chunks.size();
}

void World::increaseRenderDistance()
{
	_renderDistance += 2;
	if (_renderDistance > _maxRender)
		_renderDistance = _maxRender;
}

void World::decreaseRenderDistance()
{
	_renderDistance -= 2;
	if (_renderDistance < 1)
		_renderDistance = 1;
}

void World::pushVerticesToOpenGL(bool isTransparent) {
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

	if (_ssboData.size() != 0) {
		glNamedBufferSubData(
			_ssbo,
			0,
			sizeof(ivec4) * _ssboData.size(),
			_ssboData.data()
		);
	}

	glBindBuffer(GL_ARRAY_BUFFER, _instanceVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(int) * _vertexData.size(), _vertexData.data(), GL_STATIC_DRAW);
	_needUpdate = false;
}

void World::clearFaces() {
	_vertexData.clear();
	_ssboData.clear();
	_indirectBufferData.clear();
	// _transparentVertexData.clear();
	// _hasSentFaces = false;
	// _needTransparentUpdate = true;
}

void World::initGLBuffer()
{
	if (_hasBufferInitialized == true)
		return ;
	glGenVertexArrays(1, &_vao);
	glGenBuffers(1, &_vbo);
	glGenBuffers(1, &_instanceVBO);
	glGenBuffers(1, &_indirectBuffer);
	glCreateBuffers(1, &_ssbo);

    GLint vertices[] = {
        0, 0, 0,
        1, 0, 0,
        0, 1, 0,
        1, 1, 0,
    };

    glBindVertexArray(_vao);
    glBindBuffer(GL_ARRAY_BUFFER, _vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glVertexAttribIPointer(0, 3, GL_INT, 3 * sizeof(GLint), (void*)0); // Positions
    glEnableVertexAttribArray(0);
	
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
