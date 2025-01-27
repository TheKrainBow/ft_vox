#include "World.hpp"
#include "define.hpp"
#include <cmath>
#include <iostream>
#include <memory>
#include <unordered_map>
#include <tuple>

// Helper function to calculate block position within a chunk
vec3 World::calculateBlockPos(int x, int y, int z) const {
	auto mod = [](int value) { return (value >= 0) ? value % CHUNK_SIZE : (CHUNK_SIZE + (value % CHUNK_SIZE)) % CHUNK_SIZE; };
	return { mod(x), mod(y), mod(z) };
}

void World::findOrLoadChunk(vec3 position, std::unordered_map<std::tuple<int, int, int>, std::unique_ptr<Chunk>>& tempChunks)
{
    NoiseGenerator::PerlinMap *perlinMap = nullptr;
    perlinMap = _perlinGenerator.getPerlinMap(position.x, position.z);
    if (!perlinMap)
        return ;
    if (perlinMap->heighest < (position.y - 1) * CHUNK_SIZE)
        return ;
    auto currentTuple = std::make_tuple((int)position.x, (int)position.y, (int)position.z);
	auto it = _loadedChunks.find(currentTuple);
	if (it != _loadedChunks.end())
	{
		tempChunks[currentTuple] = std::move(it->second);
		_loadedChunks.erase(it);
	}
	//else if (auto it = _cachedChunks.find(currentTuple); it != _cachedChunks.end())
	//{
	//	tempChunks[currentTuple] = std::move(it->second);
	//	_cachedChunks.erase(it);
	//}
	else
	{
		auto newChunk = std::make_unique<Chunk>(position.x, position.y, position.z, perlinMap, *this);
		tempChunks[currentTuple] = std::move(newChunk);
	}
}

World::World(int seed) : _perlinGenerator(seed) {
	glGenVertexArrays(1, &_vao);
	glGenBuffers(1, &_vbo);
	glGenBuffers(1, &_instanceVBO);
	glGenBuffers(1, &_indirectBuffer);
	glGenBuffers(1, &_ssbo);

    GLfloat vertices[] = {
        0, 0, 0,
        1, 0, 0,
        0, 1, 0,
        1, 1, 0,
    };

    glBindVertexArray(_vao);
    glBindBuffer(GL_ARRAY_BUFFER, _vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), (void*)0); // Positions
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
}

NoiseGenerator &World::getNoiseGenerator(void)
{
    return (_perlinGenerator);
}

World::~World() = default;

void World::loadPerlinMap(vec3 camPosition)
{
    _perlinGenerator.clearPerlinMaps();
	vec2 position;
	for (int x = -XZ_RENDER_DISTANCE; x < XZ_RENDER_DISTANCE; x++)
	{
        for (int z = -XZ_RENDER_DISTANCE; z < XZ_RENDER_DISTANCE; z++)
        {
            position.x = trunc(camPosition.x / CHUNK_SIZE) + x;
            position.y = trunc(camPosition.z / CHUNK_SIZE) + z;
            _perlinGenerator.addPerlinMap(position.x, position.y, CHUNK_SIZE, 1);
        }
	}
}

void World::loadChunk(vec3 camPosition)
{
	std::unordered_map<std::tuple<int, int, int>, std::unique_ptr<Chunk>> tempChunks;

	(void)camPosition;
	//findOrLoadChunk(vec3(0, 0, 0), tempChunks);
	 vec3 position;
	 for (int x = -XZ_RENDER_DISTANCE; x < XZ_RENDER_DISTANCE; x++)
	 {
	 	for (int y = -Y_RENDER_DISTANCE; y < Y_RENDER_DISTANCE; y++)
	 	{
	 		for (int z = -XZ_RENDER_DISTANCE; z < XZ_RENDER_DISTANCE; z++)
	 		{
	 			position.x = trunc(camPosition.x / CHUNK_SIZE) + x;
	 			position.y = trunc(camPosition.y / CHUNK_SIZE) + y;
	 			position.z = trunc(camPosition.z / CHUNK_SIZE) + z;
	 			findOrLoadChunk(position, tempChunks);
	 		}
	 	}
	 }
	//_cachedChunks.insert(std::make_move_iterator(_loadedChunks.begin()), std::make_move_iterator(_loadedChunks.end()));
	_loadedChunks = std::move(tempChunks);
}

char World::getBlock(int x, int y, int z)
{
    vec3 chunkPos(x / CHUNK_SIZE, y / CHUNK_SIZE, z / CHUNK_SIZE);
    chunkPos.x -= (x < 0 && abs(x) % CHUNK_SIZE != 0);
    chunkPos.y -= (y < 0 && abs(y) % CHUNK_SIZE != 0);
    chunkPos.z -= (z < 0 && abs(z) % CHUNK_SIZE != 0);

    Chunk* chunk = getChunk((int)chunkPos.x, (int)chunkPos.y, (int)chunkPos.z);
    if (!chunk) return 'D';

    vec3 blockPos = calculateBlockPos(x, y, z);
    return chunk->getBlock((int)blockPos.x, (int)blockPos.y, (int)blockPos.z);
}

Chunk* World::getChunk(int chunkX, int chunkY, int chunkZ)
{
    auto it = _loadedChunks.find(std::make_tuple(chunkX, chunkY, chunkZ));
    if (it != _loadedChunks.end())
        return it->second.get();
    // auto itt = _cachedChunks.find(std::make_tuple(chunkX, chunkY, chunkZ));
    // if (itt != _cachedChunks.end())
    //     return itt->second.get();
    return nullptr;
}

void World::sendFacesToDisplay()
{
	_indirectCmds.clear();
	//int i = 0;
    for (auto& chunk : _loadedChunks)
	{
        if (chunk.second)
		{
			DrawArraysIndirectCommand cmd = {};
            chunk.second->sendFacesToDisplay();
			cmd.count = 4;
			cmd.instanceCount = chunk.second->getBufferLenght();
			cmd.baseInstance = chunk.second->getStartIndex();
			cmd.first = 0;
			_indirectCmds.push_back(cmd);
		}
    }
	setupBuffers();
	updateSSBO();
}

int	World::getLoadedChunksNumber()
{
	return _loadedChunks.size();
}

//int	World::getCachedChunksNumber()
//{
//	return _cachedChunks.size();
//}

void World::setupBuffers() {

    if (_vertexData.empty()) return;

    glBindVertexArray(_vao);

    // Instance data (instancePositions)
    glBindBuffer(GL_ARRAY_BUFFER, _instanceVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(int) * _vertexData.size(), _vertexData.data(), GL_STATIC_DRAW);

    glVertexAttribIPointer(1, 1, GL_INT, sizeof(int), (void*)0); // Instance positions
    glEnableVertexAttribArray(1);
    glVertexAttribDivisor(1, 1); // Update once per instance
    glBindBuffer(GL_ARRAY_BUFFER, 0);

	// Create the Indirect Command Buffer
	glBindBuffer(GL_DRAW_INDIRECT_BUFFER, _indirectBuffer);
	glBufferData(GL_DRAW_INDIRECT_BUFFER, _indirectCmds.size() * sizeof(DrawArraysIndirectCommand), _indirectCmds.data(), GL_STATIC_DRAW);

	glBindBuffer(GL_SHADER_STORAGE_BUFFER, _ssbo);
	glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(vec4) * _chunkPositions.size(), _chunkPositions.data(), GL_DYNAMIC_DRAW);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, _ssbo);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
	glBindVertexArray(0);
}

int World::display(void)
{
	if (_vertexData.empty())
		return 0;
	glBindVertexArray(_vao);
	glMultiDrawArraysIndirect(GL_TRIANGLE_STRIP, (void*)0, _indirectCmds.size(), 0);

	//for (auto &chunk: _loadedChunks)
	//{
	//	if (chunk.second)
	//		glDrawArraysInstancedBaseInstance(GL_TRIANGLE_STRIP, 0, 4, chunk.second->getStartIndex(), chunk.second->getBufferLenght());
	//}

	int test = 0;
	for (auto &cmd: _indirectCmds)
	{
		test += cmd.instanceCount;
		//glDrawArraysInstancedBaseInstance(GL_TRIANGLE_STRIP, 0, 4, cmd.baseInstance, cmd.instanceCount);
	}

	//std::cout << _vertexData.size() << ", " << _chunkPositions.size() << ", " << _vertexSize << ", " << test << std::endl;
	glBindVertexArray(0);
	return (_vertexSize * 2);
}

void World::addVertex(int vertexData, vec3 chunkPosition)
{
	(void)chunkPosition;
	_vertexData.push_back(vertexData);
	//_chunkPositions.push_back(chunkPosition);
	_chunkPositions.push_back(vec4(chunkPosition.x, chunkPosition.y, chunkPosition.z, 400));
	_vertexSize++;
}

int World::getVertexSize(void)
{
	return _vertexSize;
}

void World::resizeSSBO(size_t newChunkCount)
{
    // Get the current size of the buffer
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, _ssbo);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(glm::vec3) * newChunkCount, nullptr, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void World::updateSSBO()
{
    // Resize the SSBO to match the new chunk count
    //resizeSSBO(1024);

	// Update the UBO with the new chunk positions
	//glBindBuffer(GL_SHADER_STORAGE_BUFFER, _ssbo);
	//glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(glm::vec3) * _chunkPositions.size(), _chunkPositions.data(), GL_DYNAMIC_DRAW);
	//glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    // Map the SSBO to a pointer and update the chunk positions
    //glBindBuffer(GL_SHADER_STORAGE_BUFFER, _ssbo);
    //glm::vec3* ptr = (glm::vec3*)glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, sizeof(glm::vec3) * 1024,
    //                                              GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
    //if (ptr) {
    //    size_t i = 0;
    //    for (auto& chunk : _loadedChunks) {
    //        ptr[i++] = chunk.second->getWorldPosition();  // Get the chunk's position and store it in the SSBO
    //    }
    //    glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);  // Make sure to unmap after writing
    //}
	//size_t i = 0;
	//for (auto& chunk : _loadedChunks) {
	//	(void)chunk;
	//	std::cout << "ptr[" << i << "] (" << ptr[i].x << ", " << ptr[i].y << ", " << ptr[i].z << ")" << std::endl;  // Get the chunk's position and store it in the SSBO
	//	i++;
	//}
    //glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}