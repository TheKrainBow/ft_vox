#include "World.hpp"

World::World(int seed, TextureManager &textureManager, Camera &camera, ThreadPool &pool) : _threadPool(pool), _textureManager(textureManager), _camera(camera), _perlinGenerator(seed)
{
	_needUpdate = true;
	_needTransparentUpdate = true;
	_hasBufferInitialized = false;
	_renderDistance = RENDER_DISTANCE;
	_drawnSSBOSize = 1;
	_currentRender = 0;

	_memorySize = 0;

	_drawData = nullptr;
	_transparentDrawData = nullptr;

	// Load first chunk under the player, and pop their position on top of it
	ivec2 chunkPos = camera.getChunkPosition(CHUNK_SIZE);
	vec3 worldPos = camera.getWorldPosition();
	loadChunk(0, 0, 0, chunkPos, 1);
	int height = findTopBlockY(chunkPos, {worldPos.x, worldPos.z});
	const vec3 &camPos = camera.getPosition();
	camera.setPos({camPos.x, -(height+3), camPos.z});
}

int World::findTopBlockY(ivec2 chunkPos, ivec2 worldPos) {
	Chunk* chunk = _chunks[{chunkPos.x, chunkPos.y}];
	if (!chunk) return 0;
	int localX = (worldPos.x % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;
	int localZ = (worldPos.y % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;
	return chunk->getTopBlock(localX, localZ);
}

void World::init(int renderDistance = RENDER_DISTANCE) {
	_renderDistance = renderDistance;
	initGLBuffer();
}

World::~World()
{
	std::lock_guard<std::mutex> lock(_chunksMutex);
	for (auto it = _chunks.begin(); it != _chunks.end(); it++)
	{
		it->second->freeSubChunks();
		delete it->second;
	}
	if (_drawData)
		delete _drawData;
	if (_transparentDrawData)
		delete _transparentDrawData;
	if (_transparentFillData)
		delete _transparentFillData;
	if (_transparentStagingData)
		delete _transparentStagingData;
}

NoiseGenerator &World::getNoiseGenerator()
{
	return (_perlinGenerator);
}

int *World::getRenderDistancePtr()
{
	return &_renderDistance;
}

BlockType World::getBlock(ivec2 &chunkPos, ivec3 &worldPos)
{
	std::lock_guard<std::mutex> lock(_chunksMutex);
	Chunk *chunk = _chunks[{chunkPos.x, chunkPos.y}];
	if (!chunk)
		return AIR;
	int subChunkIndex = static_cast<int>(floor(worldPos.y / CHUNK_SIZE));
	SubChunk *subchunk = chunk->getSubChunk(subChunkIndex);
	if (!subchunk)
		return AIR;
	int localX = (worldPos.x % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;
	int localY = (worldPos.y % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;
	int localZ = (worldPos.z % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;
	return subchunk->getBlock({localX, localY, localZ});
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
	_chunksListMutex.lock();
	size_t chunksNb = _chunkList.size();
	_chunksListMutex.unlock();

	if (chunksNb <= CACHE_SIZE)
		return ;

	// Get player position in chunk coordinates
	ivec3 playerPos = _camera.getWorldPosition();
	int playerChunkX = playerPos.x / CHUNK_SIZE;
	int playerChunkZ = playerPos.z / CHUNK_SIZE;
	if (playerPos.x < 0) playerChunkX--;
	if (playerPos.z < 0) playerChunkZ--;

	// Find the farthest chunk
	_chunksListMutex.lock();
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
		
		// Check if chunk is being displayed
		_displayedChunksMutex.lock();
		bool isDisplayed = false;
		auto endDisplay = _displayedChunks.end();
		auto displayIt = _displayedChunks.find(pos);
		isDisplayed = endDisplay != displayIt;
		_displayedChunksMutex.unlock();
		if (isDisplayed)
			return ;

		// Remove from _chunks
		_chunksMutex.lock();
		_chunks.erase(pos);
		_chunksMutex.unlock();

		// Clean up memory
		_perlinGenerator.removePerlinMap(pos.x, pos.y);
		chunkToRemove->unloadNeighbors();
		chunkToRemove->freeSubChunks();
		delete chunkToRemove;
	}
	else
	{
		_chunksListMutex.unlock();
	}
}

Chunk *World::loadChunk(int x, int z, int render, ivec2 &chunkPos, int resolution)
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
		if (chunk->_resolution > resolution)
			chunk->updateResolution(resolution);
	}
	else
	{
		PerlinMap *pMap = _perlinGenerator.getPerlinMap(pos, resolution);
		chunk = new Chunk(pos, pMap, *this, _textureManager, resolution);
		_chunks[pos] = chunk;
		_chunksMutex.unlock();
		chunk->loadBlocks();
		chunk->getNeighbors();
		_memorySize += chunk->getMemorySize();
		_chunksListMutex.lock();
		_chunkList.emplace_back(chunk);
		_chunksListMutex.unlock();
	}
	_displayedChunksMutex.lock();
	_displayedChunks[pos] = chunk;
	_displayedChunksMutex.unlock();
	unloadChunk();
	return chunk;
}

void World::loadTopChunks(int render, ivec2 &chunkPos, int resolution)
{
	int z = 0;
	for (int x = 0; x < render && getIsRunning(); x++)
	{
		loadChunk(x, z, render, chunkPos, resolution);
	}
}

void World::loadBotChunks(int render, ivec2 &chunkPos, int resolution)
{
	int z = render - 1;
	for (int x = render - 1; getIsRunning() && x >= 0; x--)
	{
		loadChunk(x, z, render, chunkPos, resolution);
	}
}

void World::loadRightChunks(int render, ivec2 &chunkPos, int resolution)
{
	int x = render - 1;
	for (int z = 1; z < render - 1 && getIsRunning(); z++) // avoid corners
	{
		loadChunk(x, z, render, chunkPos, resolution);
	}
}

void World::loadLeftChunks(int render, ivec2 &chunkPos, int resolution)
{
	int x = 0;
	for (int z = render - 2; getIsRunning() && z > 0; z--) // avoid corners
	{
		loadChunk(x, z, render, chunkPos, resolution);
	}
}

void World::loadFirstChunks(ivec2 &chunkPos)
{
	int renderDistance = _renderDistance;

	int resolution = RESOLUTION;
	_threshold = LOD_THRESHOLD;
	// _chronoHelper.startChrono(1, "Build chunks + loaded faces");
	// _chronoHelper.stopChrono(1);
	// _chronoHelper.printChrono(1);
	std::vector<std::future<void>> retLst;
	_currentRender = 0;
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

		if (render >= _threshold && resolution < CHUNK_SIZE)
		{
			resolution *= 2;
			_threshold = _threshold * 2;
		}
		retLst.emplace_back(_threadPool.enqueue(&World::updateFillData, this));
		if (hasMoved(chunkPos))
			break;
		_currentRender = render + 2;
    }

	for (std::future<void> &ret : retLst)
	{
		ret.get();
	}
	retLst.clear();
}

int *World::getCurrentRenderPtr() {
	return &_currentRender;
}

size_t *World::getMemorySizePtr() {
	return &_memorySize;
}

void World::unLoadNextChunks(ivec2 &newCamChunk)
{
	ivec2 pos;
	std::queue<ivec2> deleteQueue;
	_displayedChunksMutex.lock();
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
	_displayedChunksMutex.unlock();
	while (!deleteQueue.empty())
	{
		pos = deleteQueue.front();
		_displayedChunksMutex.lock();
		_displayedChunks.erase(pos);
		_displayedChunksMutex.unlock();
		deleteQueue.pop();
	}
	updateFillData();
}

void World::updateFillData()
{
	DisplayData *fillData = new DisplayData();
	DisplayData *transparentData = new DisplayData();
	buildFacesToDisplay(fillData, transparentData);
	_drawDataMutex.lock();
	_stagedDataQueue.emplace(fillData);
	_transparentStagedDataQueue.emplace(transparentData);
	_drawDataMutex.unlock();
}

bool World::hasMoved(ivec2 &oldPos)
{
	ivec2 camChunk = _camera.getChunkPosition(CHUNK_SIZE);

	if (((floor(oldPos.x) != floor(camChunk.x) || floor(oldPos.y) != floor(camChunk.y))))
		return true;
	return false;
}

SubChunk *World::getSubChunk(ivec3 &position)
{
	_chunksMutex.lock();
	auto it = _chunks.find(ivec2(position.x, position.z));
	auto itend = _chunks.end();
	_chunksMutex.unlock();
	if (it != itend)
		return it->second->getSubChunk(position.y);
	return nullptr;
}

Chunk *World::getChunk(ivec2 &position)
{
	_chunksMutex.lock();
	auto it = _chunks.find({position.x, position.y});
	auto itend = _chunks.end();
	_chunksMutex.unlock();
	if (it != itend)
		return it->second;
	return nullptr;
}

void World::buildFacesToDisplay(DisplayData *fillData, DisplayData *transparentFillData)
{
	// Copy of the unordered map of displayable chunks
	std::unordered_map<ivec2, Chunk*, ivec2_hash> displayedChunks;
	_displayedChunksMutex.lock();
	displayedChunks = _displayedChunks;
	_displayedChunksMutex.unlock();

	for (auto &chunk : displayedChunks)
	{
		// Fill solid vertices
		size_t size = fillData->vertexData.size();
		std::vector<int> vertices = chunk.second->getVertices();
		fillData->vertexData.insert(fillData->vertexData.end(), vertices.begin(), vertices.end());

		// Fill solid indirect buffers
		std::vector<DrawArraysIndirectCommand> indirectBufferData = chunk.second->getIndirectData();
		for (DrawArraysIndirectCommand &tmp : indirectBufferData) {
			tmp.baseInstance += size;
		}
		fillData->indirectBufferData.insert(fillData->indirectBufferData.end(), indirectBufferData.begin(), indirectBufferData.end());

		// Fill transparent vertices
		size_t transparentSize = transparentFillData->vertexData.size();
		std::vector<int> transparentVertices = chunk.second->getTransparentVertices();
		transparentFillData->vertexData.insert(transparentFillData->vertexData.end(), transparentVertices.begin(), transparentVertices.end());

		// Fill transparent indirect buffers
		std::vector<DrawArraysIndirectCommand> transparentIndirectBuffer = chunk.second->getTransparentIndirectData();
		for (DrawArraysIndirectCommand &tmp : transparentIndirectBuffer) {
			tmp.baseInstance += transparentSize;
		}
		transparentFillData->indirectBufferData.insert(transparentFillData->indirectBufferData.end(), transparentIndirectBuffer.begin(), transparentIndirectBuffer.end());

		// SSBO load (same for both solid and transparent we arbitrarily use _fillData/_drawData)
		std::vector<vec4> ssboData = chunk.second->getSSBO();
		fillData->ssboData.insert(fillData->ssboData.end(), ssboData.begin(), ssboData.end());
	}
}

void World::updateSSBO()
{
	// SSBO Update
	bool needUpdate = false;
	size_t size = _drawData->ssboData.size() * 2;
	while (size > _drawnSSBOSize)
	{
		_drawnSSBOSize *= 2;
		needUpdate = true;
	}
	if (needUpdate)
	{
		glDeleteBuffers(1, &_ssbo);
		glCreateBuffers(1, &_ssbo);
		glNamedBufferStorage(_ssbo, 
			sizeof(vec4) * _drawnSSBOSize, 
			nullptr, 
			GL_DYNAMIC_STORAGE_BIT);
		glNamedBufferSubData(
			_ssbo,
			0,
			sizeof(vec4) * _drawData->ssboData.size(),
			_drawData->ssboData.data()
		);
	}
}

void World::updateDrawData()
{
	// Lock here
	_drawDataMutex.lock();
	if (!_stagedDataQueue.empty())
	{
		DisplayData *stagedData = _stagedDataQueue.front();
		std::swap(stagedData, _drawData);
		delete stagedData;
		stagedData = nullptr;
		_stagedDataQueue.pop();
		_needUpdate = true;
	}
	if (!_transparentStagedDataQueue.empty())
	{
		
		DisplayData *stagedData = _transparentStagedDataQueue.front();
		std::swap(stagedData, _transparentDrawData);
		delete stagedData;
		stagedData = nullptr;
		_transparentStagedDataQueue.pop();
		_needTransparentUpdate = true;
	}
	if (_drawData && (_needUpdate || _needTransparentUpdate))
		updateSSBO();
}

int World::display()
{
	if (!_drawData)
		return 0;
	if (_needUpdate)
	{
		pushVerticesToOpenGL(false);
		_needUpdate = false;
	}
	long long size = _drawData->vertexData.size();
	if (size == 0)
		return 0;

	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, _ssbo);
	glBindVertexArray(_vao);
	glBindBuffer(GL_DRAW_INDIRECT_BUFFER, _indirectBuffer);

	glMultiDrawArraysIndirect(GL_TRIANGLE_STRIP, nullptr, _drawData->indirectBufferData.size(), 0);
	glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
	glBindVertexArray(0);
	return (size * 2);
}

int World::displayTransparent()
{
	if (!_transparentDrawData)
	{
		_drawDataMutex.unlock();
		return 0;
	}
	if (_needTransparentUpdate)
	{
		pushVerticesToOpenGL(true);
		_needTransparentUpdate = false;
	}
	glDisable(GL_CULL_FACE);
	long long size = _transparentDrawData->vertexData.size();
	if (size == 0) {
		_drawDataMutex.unlock();
		return 0;
	}

	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, _ssbo);
	glBindVertexArray(_transparentVao);
	glBindBuffer(GL_DRAW_INDIRECT_BUFFER, _transparentIndirectBuffer);

	glMultiDrawArraysIndirect(GL_TRIANGLE_STRIP, nullptr, _transparentDrawData->indirectBufferData.size(), 0);

	glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
	glBindVertexArray(0);
	// Unlock here
	_drawDataMutex.unlock();
	return (size * 2);
}

void World::pushVerticesToOpenGL(bool isTransparent)
{
	if (isTransparent)
	{
		glBindBuffer(GL_DRAW_INDIRECT_BUFFER, _transparentIndirectBuffer);
		glBufferData(GL_DRAW_INDIRECT_BUFFER, sizeof(DrawArraysIndirectCommand) * _transparentDrawData->indirectBufferData.size(), _transparentDrawData->indirectBufferData.data(), GL_STATIC_DRAW);
		glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);

		if (_drawData->ssboData.size() != 0) {
			glNamedBufferSubData(
				_ssbo,
				0,
				sizeof(vec4) * _drawData->ssboData.size(),
				_drawData->ssboData.data()
			);
		}

		glBindBuffer(GL_ARRAY_BUFFER, _transparentInstanceVBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(int) * _transparentDrawData->vertexData.size(), _transparentDrawData->vertexData.data(), GL_STATIC_DRAW);
		_needTransparentUpdate = false;
	}
	else
	{
		glBindBuffer(GL_DRAW_INDIRECT_BUFFER, _indirectBuffer);
		glBufferData(GL_DRAW_INDIRECT_BUFFER, sizeof(DrawArraysIndirectCommand) * _drawData->indirectBufferData.size(), _drawData->indirectBufferData.data(), GL_STATIC_DRAW);
		glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);

		if (_drawData->ssboData.size() != 0) {
			glNamedBufferSubData(
				_ssbo,
				0,
				sizeof(vec4) * _drawData->ssboData.size(),
				_drawData->ssboData.data()
			);
		}

		glBindBuffer(GL_ARRAY_BUFFER, _instanceVBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(int) * _drawData->vertexData.size(), _drawData->vertexData.data(), GL_STATIC_DRAW);
		_needUpdate = false;
	}
}

// void World::clearFaces() {
// 	// Clear solid data
// 	_fillData->vertexData.clear();
// 	_fillData->indirectBufferData.clear();

// 	// Clear transparent data
// 	_transparentFillData->vertexData.clear();
// 	_transparentFillData->indirectBufferData.clear();

// 	// Clear common ssbo
// 	_fillData->ssboData.clear();
// }

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

void World::updatePerlinMapResolution(PerlinMap *pMap, int newResolution)
{
	_perlinGenerator.updatePerlinMapResolution(pMap, newResolution);
}
