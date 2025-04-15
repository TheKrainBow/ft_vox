#pragma once
#include "ft_vox.hpp"
#include "NoiseGenerator.hpp"
#include "SubChunk.hpp"
#include "Chunk.hpp"
#include "Camera.hpp"
#include "Chrono.hpp"
#include "ThreadPool.hpp"
#include <map>
#include <atomic>

class Chunk;

namespace std {
    template <>
    struct hash<std::pair<int, int>> {
        size_t operator()(const std::pair<int, int>& t) const {
            size_t h1 = std::hash<int>{}(std::get<0>(t));
            size_t h2 = std::hash<int>{}(std::get<1>(t));
            return h1 ^ (h2 << 1);
        }
    };
}

struct ChunkSlot
{
	Chunk *chunk;
	std::mutex mutex;
};

class SubChunk;

struct ChunkElement
{
	Chunk *chunk;
	int priority;
	int displayPos;
};

struct Compare {
    bool operator()(const ChunkElement& a, const ChunkElement& b) const {
        return a.priority > b.priority;
    }
};

struct DisplayData
{
	std::vector<vec4>						ssboData;
	std::vector<int>						vertexData;
	std::vector<DrawArraysIndirectCommand>	indirectBufferData;
};

class World
{
private:
	// World related informations
	std::unordered_map<vec2, Chunk*, vec2_hash>	_chunks;
	std::unordered_map<vec2, Chunk*, vec2_hash>	_displayedChunks;
	std::mutex _displayedChunksMutex;
	std::list<Chunk *>							_chunkList;
	std::mutex									_chunksListMutex;

	std::unordered_map<vec2, Chunk*, vec2_hash> _activeChunks;
	
	bool										_skipLoad;
	ThreadPool 									&_threadPool;
	TextureManager								&_textureManager;
	
	// Player related informations
	Camera										*_camera;
	int											_renderDistance;
	int											_maxRender = 1000;
	bool										*_isRunning;
	std::mutex									*_runningMutex;
	std::atomic_bool							displayReady;
	Chrono chronoHelper;

	bool 									_hasBufferInitialized;
	GLuint									_ssbo;

	// Display
	std::mutex								_drawDataMutex;
	DisplayData								*_drawData;
	DisplayData								*_fillData;
	DisplayData								*_stagingData;

	DisplayData								*_transparentDrawData;
	DisplayData								*_transparentFillData;
	DisplayData								*_transparentStagingData;

	size_t									_drawnSSBOSize;
	GLuint									_instanceVBO;
	GLuint									_indirectBuffer;
	GLuint									_vao;
	GLuint									_vbo;

	bool 									_hasTransparentBufferInitialized;
	GLuint									_transparentVao;
	GLuint									_transparentInstanceVBO;
	GLuint									_transparentIndirectBuffer;

	bool									_needUpdate;
	bool									_needTransparentUpdate;
	std::atomic_int 						_threshold;
public:
	NoiseGenerator								_perlinGenerator;
	GLuint 										_shaderProgram;
	std::mutex									_chunksMutex;
	World(int seed, TextureManager &textureManager, Camera &camera, ThreadPool &pool);
	~World();
	void loadFirstChunks(vec2 camPosition);
	void init(GLuint shaderProgram, int renderDistance);
	
	void unLoadNextChunks(vec2 newCamChunk);
	void loadChunk(int x, int z, int render, vec2 chunkPos, int resolution, Direction dir);
	void loadPerlinMap(vec3 camPosition);
	NoiseGenerator &getNoiseGenerator(void);
	int	getCachedChunksNumber();
	Chunk* getChunk(vec2 position);
	SubChunk* getSubChunk(ivec3 position);
	void updateActiveChunks();
	int display();
	int displayTransparent();
	void increaseRenderDistance();
	void decreaseRenderDistance();
	int *getRenderDistancePtr();
	void setRunning(std::mutex *runningMutex, bool *isRunning);
	void updateDrawData();
private:
	ivec3 calculateBlockPos(ivec3 position) const;
	bool getIsRunning();
	void loadTopChunks(int render, vec2 camPosition, int resolution = 1);
	void loadRightChunks(int render, vec2 camPosition, int resolution = 1);
	void loadBotChunks(int render, vec2 camPosition, int resolution = 1);
	void loadLeftChunks(int render, vec2 camPosition, int resolution = 1);
	void unloadChunk();
	bool hasMoved(vec2 oldPos);

	void pushVerticesToOpenGL(bool isTransparent);
	void clearFaces();
	void clearTransparentFaces();
	void buildFacesToDisplay();
	void updateSSBO();
	void updateFillData();
	void initGLBuffer();
};
