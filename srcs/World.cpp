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
glm::ivec3 World::calculateBlockPos(glm::ivec3 position) const
{
	auto mod = [](int value) { return (value >= 0) ? value % CHUNK_SIZE : (CHUNK_SIZE + (value % CHUNK_SIZE)) % CHUNK_SIZE; };
	return { mod(position.x), mod(position.y), mod(position.z) };
}

World::World(int seed, TextureManager &textureManager, Camera &camera, ThreadPool &pool) : _threadPool(pool), _textureManager(textureManager), _camera(&camera), _perlinGenerator(seed)
{
	_newDataReady = false;
	_needUpdate = true;
	_needTransparentUpdate = true;
	_hasBufferInitialized = false;
	_renderDistance = RENDER_DISTANCE;
	_drawnSSBOSize = 1;

	_fillData = new DisplayData;
	_drawData = new DisplayData;
	_stagingData = new DisplayData;

	_transparentDrawData = new DisplayData;
	_transparentFillData = new DisplayData;
	_transparentStagingData = new DisplayData;
}

void World::init(GLuint shaderProgram, int renderDistance = RENDER_DISTANCE) {
	_shaderProgram = shaderProgram;
	_renderDistance = renderDistance;
	initGLBuffer();
}

World::~World()
{
	std::lock_guard<std::mutex> lock(_chunksMutex);
	for (auto it = _chunks.begin(); it != _chunks.end(); it++)
		delete it->second;
	// Free solid blocks data
	delete _fillData;
	delete _drawData;
	delete _stagingData;
	
	// Free transparent blocks data
	delete _transparentDrawData;
	delete _transparentFillData;
	delete _transparentStagingData;
}

NoiseGenerator &World::getNoiseGenerator(void)
{
	return (_perlinGenerator);
}

void World::loadPerlinMap(glm::vec3 camPosition)
{
	_perlinGenerator.clearPerlinMaps();
	glm::vec2 position;
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
	glm::vec3 playerPos = _camera->getWorldPosition();
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

		glm::vec2 pos = chunkToRemove->getPosition();

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

void World::loadChunk(int x, int z, int render, glm::vec2 chunkPos, int resolution, Direction dir)
{
	Chunk *chunk = nullptr;
	glm::vec2 pos = {chunkPos.x - render / 2 + x, chunkPos.y - render / 2 + z};
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
	_displayedChunksMutex.lock();
	_displayedChunks[pos] = chunk;
	_displayedChunksMutex.unlock();
	// unloadChunk();
}

void World::loadTopChunks(int render, glm::vec2 chunkPos, int resolution)
{
	int z = 0;
	for (int x = 0; x < render && getIsRunning(); x++)
	{
		loadChunk(x, z, render, chunkPos, resolution, NORTH);
	}
}

void World::loadBotChunks(int render, glm::vec2 chunkPos, int resolution)
{
	int z = render - 1;
	for (int x = render - 1; getIsRunning() && x >= 0; x--)
	{
		loadChunk(x, z, render, chunkPos, resolution, SOUTH);
	}
}

void World::loadRightChunks(int render, glm::vec2 chunkPos, int resolution)
{
	int x = render - 1;
	for (int z = 0; z < render && getIsRunning(); z++)
	{
		loadChunk(x, z, render, chunkPos, resolution, EAST);
	}
}

void World::loadLeftChunks(int render, glm::vec2 chunkPos, int resolution)
{
	int x = 0;
	for (int z = render - 1; getIsRunning() && z >= 0; z--)
	{
		loadChunk(x, z, render, chunkPos, resolution, WEST);
	}
}

void World::loadFirstChunks(glm::vec2 chunkPos)
{
	int renderDistance = _renderDistance;
	_skipLoad = false;

	int resolution = RESOLUTION;
	_threshold = LOD_THRESHOLD;
	std::vector<std::future<void>> retLst;
    for (int render = 0; getIsRunning() && render < renderDistance; render += 2)
	{
		std::future<void> retTop;
		std::future<void> retBot;
		std::future<void> retRight;
		std::future<void> retLeft;

		// Load chunks
		retTop = _threadPool.enqueue(&World::loadTopChunks, this, render, chunkPos, resolution);
		retBot = _threadPool.enqueue(&World::loadRightChunks, this, render, chunkPos, resolution);
		retRight = _threadPool.enqueue(&World::loadBotChunks, this, render, chunkPos, resolution);
		retLeft = _threadPool.enqueue(&World::loadLeftChunks, this, render, chunkPos, resolution);

		retTop.get();
		retBot.get();
		retRight.get();
		retLeft.get();

		if (render >= _threshold && resolution < 32)
		{
			resolution *= 2;
			_threshold = _threshold * 2;
		}
		updateFillData();
		if (hasMoved(chunkPos))
			break;
    }

	// for (std::future<void> &ret : retLst)
	// {
	// 	ret.get();
	// }
	// retLst.clear();
}

void World::unLoadNextChunks(glm::vec2 newCamChunk)
{
	glm::vec2 pos;
	std::queue<glm::vec2> deleteQueue;
	for (auto &it : _displayedChunks)
	{
		Chunk *chunk = it.second;
		glm::vec2 chunkPos = chunk->getPosition();
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
		_displayedChunks.erase(pos);
		deleteQueue.pop();
	}
	updateFillData();
}

void World::updateFillData()
{
	sendFacesToDisplay();
	_drawDataMutex.lock();
	std::swap(_fillData, _stagingData);
	_newDataReady = true;
	_drawDataMutex.unlock();
}

bool World::hasMoved(glm::vec2 oldPos)
{
	glm::vec2 camChunk = _camera->getChunkPosition(CHUNK_SIZE);

	if (((floor(oldPos.x) != floor(camChunk.x) || floor(oldPos.y) != floor(camChunk.y))))
		return true;
	return false;
}

SubChunk *World::getSubChunk(glm::ivec3 position)
{
	_chunksMutex.lock();
	auto it = _chunks.find(glm::vec2(position.x, position.z));
	auto itend = _chunks.end();
	_chunksMutex.unlock();
	if (it != itend)
		return it->second->getSubChunk(position.y);
	return nullptr;
}

Chunk *World::getChunk(glm::vec2 position)
{
	// _chunksMutex.lock();
	auto it = _chunks.find({position.x, position.y});
	auto itend = _chunks.end();
	// _chunksMutex.unlock();
	if (it != itend)
		return it->second;
	return nullptr;
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
	for (auto &chunk : _displayedChunks)
	{
		// Fill Solid data
		size_t size = _fillData->vertexData.size();
		std::vector<int> vertices = chunk.second->getVertices();
		_fillData->vertexData.insert(_fillData->vertexData.end(), vertices.begin(), vertices.end());
		std::vector<DrawArraysIndirectCommand> indirectBufferData = chunk.second->getIndirectData();
		for (DrawArraysIndirectCommand &tmp : indirectBufferData) {
			tmp.baseInstance += size;
		}
		_fillData->indirectBufferData.insert(_fillData->indirectBufferData.end(), indirectBufferData.begin(), indirectBufferData.end());
		std::vector<glm::vec4> ssboData = chunk.second->getSSBO();
		_fillData->ssboData.insert(_fillData->ssboData.end(), ssboData.begin(), ssboData.end());

		// Fill transparent data
		size_t transparentSize = _transparentFillData->vertexData.size();
		std::vector<int> transparentVertices = chunk.second->getTransparentVertices();
		_transparentFillData->vertexData.insert(_transparentFillData->vertexData.end(), transparentVertices.begin(), transparentVertices.end());

	}
}

void World::sendFacesToDisplay()
{
	clearFaces();
	for (auto &chunk : _activeChunks)
	{
		// Transparent blocks vertex data
		size_t transparentSize = _transparentVertexData.size();
		std::vector<int> transparentVertices = chunk.second->getTransparentVertices();
		_transparentVertexData.insert(_transparentVertexData.end(), transparentVertices.begin(), transparentVertices.end());
		
		// Transparent blocks indirect buffer
		std::vector<DrawArraysIndirectCommand> transparentIndirectBuffer = chunk.second->getTransparentIndirectData();
		for (DrawArraysIndirectCommand &tmp : transparentIndirectBuffer) {
			tmp.baseInstance += transparentSize;
		}
		_transparentIndirectBufferData.insert(_transparentIndirectBufferData.end(), transparentIndirectBuffer.begin(), transparentIndirectBuffer.end());

		// SSBO load
		std::vector<glm::vec4> ssboData = chunk.second->getSSBO();
		_ssboData.insert(_ssboData.end(), ssboData.begin(), ssboData.end());
	}

	// SSBO Update
	bool needUpdate = false;
	size_t size = _drawData->ssboData.size() * 2;
	while (size > _drawnSSBOSize) {
		_drawnSSBOSize *= 2;
		needUpdate = true;
	}
	if (needUpdate) {
		glDeleteBuffers(1, &_ssbo);
		glCreateBuffers(1, &_ssbo);
		glNamedBufferStorage(_ssbo, 
			sizeof(glm::vec4) * _drawnSSBOSize, 
			nullptr, 
			GL_DYNAMIC_STORAGE_BIT);
		glNamedBufferSubData(
			_ssbo,
			0,
			sizeof(glm::vec4) * _drawData->ssboData.size(),
			_drawData->ssboData.data()
		);
	}
}

int World::display()
{
	_drawDataMutex.lock();
	if (_hasBufferInitialized == false)
		initGLBuffer();
	if (_newDataReady)
	{
		std::swap(_stagingData, _drawData);
		pushVerticesToOpenGL(false);
		_newDataReady = false;
	}
	long long size = _vertexData.size();

	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, _ssbo);
	glBindVertexArray(_vao);
	glBindBuffer(GL_DRAW_INDIRECT_BUFFER, _indirectBuffer);

	glMultiDrawArraysIndirect(GL_TRIANGLE_STRIP, nullptr, _drawData->indirectBufferData.size(), 0);
	glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
	glBindVertexArray(0);
	updateSsbo();
	_drawDataMutex.unlock();
	return (size * 2);
}

int World::displayTransparent()
{
	glDisable(GL_CULL_FACE);
	if (_needTransparentUpdate)
		pushVerticesToOpenGL(true);
	long long size = _transparentVertexData.size();

	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, _ssbo);
	glBindVertexArray(_transparentVao);
	glBindBuffer(GL_DRAW_INDIRECT_BUFFER, _transparentIndirectBuffer);

	glMultiDrawArraysIndirect(GL_TRIANGLE_STRIP, nullptr, _transparentIndirectBufferData.size(), 0);

	glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
	glBindVertexArray(0);
	return (size * 2);
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
	if (isTransparent)
	{
		glBindBuffer(GL_DRAW_INDIRECT_BUFFER, _transparentIndirectBuffer);
		glBufferData(GL_DRAW_INDIRECT_BUFFER, sizeof(DrawArraysIndirectCommand) * _transparentIndirectBufferData.size(), _transparentIndirectBufferData.data(), GL_STATIC_DRAW);
		glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);

		if (_ssboData.size() != 0) {
			glNamedBufferSubData(
				_ssbo,
				0,
				sizeof(glm::vec4) * _ssboData.size(),
				_ssboData.data()
			);
		}

		glBindBuffer(GL_ARRAY_BUFFER, _transparentInstanceVBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(int) * _transparentVertexData.size(), _transparentVertexData.data(), GL_STATIC_DRAW);
		_needTransparentUpdate = false;
	}
	else
	{
		glBindBuffer(GL_DRAW_INDIRECT_BUFFER, _indirectBuffer);
		glBufferData(GL_DRAW_INDIRECT_BUFFER, sizeof(DrawArraysIndirectCommand) * _indirectBufferData.size(), _indirectBufferData.data(), GL_STATIC_DRAW);
		glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
	
		if (_ssboData.size() != 0) {
			glNamedBufferSubData(
				_ssbo,
				0,
				sizeof(glm::vec4) * _ssboData.size(),
				_ssboData.data()
			);
		}

		glBindBuffer(GL_ARRAY_BUFFER, _instanceVBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(int) * _vertexData.size(), _vertexData.data(), GL_STATIC_DRAW);
		_needUpdate = false;
	}
}

void World::clearFaces() {
	_vertexData.clear();
	_indirectBufferData.clear();
	_transparentVertexData.clear();
	_transparentIndirectBufferData.clear();
	_ssboData.clear();
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
	
	// Static vertex data
	GLint vertices[] = {
		0, 0, 0,
		1, 0, 0,
		0, 1, 0,
		1, 1, 0,
	};
	
	// Setup solid VAO
	glBindVertexArray(_vao);
	
	// Vertex positions
	glBindBuffer(GL_ARRAY_BUFFER, _vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
	glVertexAttribIPointer(0, 3, GL_INT, 3 * sizeof(GLint), (void*)0);
	glEnableVertexAttribArray(0);
	
	// Instance data
	glBindBuffer(GL_ARRAY_BUFFER, _instanceVBO);
	glVertexAttribIPointer(2, 1, GL_INT, sizeof(int), (void*)0);
	glEnableVertexAttribArray(2);
	glVertexAttribDivisor(2, 1);
	
	glBindVertexArray(0);
	
	// Setup transparent VAO
	glGenVertexArrays(1, &_transparentVao);
	glGenBuffers(1, &_transparentInstanceVBO);
	glGenBuffers(1, &_transparentIndirectBuffer);
	
	glBindVertexArray(_transparentVao);
	
	// Vertex positions
	glBindBuffer(GL_ARRAY_BUFFER, _vbo); // shared buffer, no need to re-upload
	glVertexAttribIPointer(0, 3, GL_INT, 3 * sizeof(GLint), (void*)0);
	glEnableVertexAttribArray(0);
	
	// Instance data
	glBindBuffer(GL_ARRAY_BUFFER, _transparentInstanceVBO);
	glVertexAttribIPointer(2, 1, GL_INT, sizeof(int), (void*)0);
	glEnableVertexAttribArray(2);
	glVertexAttribDivisor(2, 1);
	
	glBindVertexArray(0);
	_hasBufferInitialized = true;
}

