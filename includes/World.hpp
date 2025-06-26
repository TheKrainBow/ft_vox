#pragma once
#include "ft_vox.hpp"
#include "NoiseGenerator.hpp"
#include "SubChunk.hpp"
#include "Chunk.hpp"
#include "Camera.hpp"
#include "Chrono.hpp"
#include "ThreadPool.hpp"

class Chunk;

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
	// Chunk loading info
	std::queue<DisplayData *>	_stagedDataQueue;
	std::queue<DisplayData *>	_transparentStagedDataQueue;
	std::list<Chunk *>							_chunkList;
	std::mutex									_chunksListMutex;
	size_t										_memorySize;
	std::unordered_map<ivec2, Chunk*, ivec2_hash>	_chunks;
	std::unordered_map<ivec2, Chunk*, ivec2_hash>	_displayedChunks;
	std::mutex										_displayedChunksMutex;

	// Chunk loading utils
	ThreadPool 									&_threadPool;
	TextureManager								&_textureManager;

	// Player related informations
	Camera										&_camera;
	int											_renderDistance;
	int											_currentRender;
	int											_maxRender = 1000;
	bool										*_isRunning;
	std::mutex									*_runningMutex;
	Chrono										_chronoHelper;


	// Display / Runtime data
	std::mutex								_drawDataMutex;
	DisplayData								*_drawData;
	GLuint									_ssbo;
	bool 									_hasBufferInitialized;

	DisplayData								*_transparentDrawData;
	DisplayData								*_transparentFillData;
	DisplayData								*_transparentStagingData;

	size_t									_drawnSSBOSize;
	GLuint									_instanceVBO;
	GLuint									_indirectBuffer;
	GLuint									_vao;
	GLuint									_vbo;

	GLuint									_transparentVao;
	GLuint									_transparentInstanceVBO;
	GLuint									_transparentIndirectBuffer;

	bool									_needUpdate;
	bool									_needTransparentUpdate;
	std::atomic_int 						_threshold;
	NoiseGenerator							_perlinGenerator;
	std::mutex								_chunksMutex;
public:
	// Init
	World(int seed, TextureManager &textureManager, Camera &camera, ThreadPool &pool);
	~World();
	void init(int renderDistance);
	void setRunning(std::mutex *runningMutex, bool *isRunning);
	
	// Display
	int display();
	int displayTransparent();

	// Runtime update
	void updatePerlinMapResolution(PerlinMap *pMap, int newResolution);
	void updateDrawData();
	
	// Runtime chunk loading/unloading
	void loadFirstChunks(ivec2 &camPosition);
	void unLoadNextChunks(ivec2 &newCamChunk);
	
	// Shared data getters
	NoiseGenerator &getNoiseGenerator(void);
	Chunk* getChunk(ivec2 &position);
	SubChunk* getSubChunk(ivec3 &position);
	size_t *getMemorySizePtr();
	int *getRenderDistancePtr();
	int *getCurrentRenderPtr();
private:
	// Chunk loading
	void loadChunk(int x, int z, int render, ivec2 &chunkPos, int resolution);
	void loadTopChunks(int render, ivec2 &camPosition, int resolution = 1);
	void loadRightChunks(int render, ivec2 &camPosition, int resolution = 1);
	void loadBotChunks(int render, ivec2 &camPosition, int resolution = 1);
	void loadLeftChunks(int render, ivec2 &camPosition, int resolution = 1);
	void unloadChunk();

	// Runtime info
	bool hasMoved(ivec2 &oldPos);
	bool getIsRunning();

	// Display
	void initGLBuffer();
	void pushVerticesToOpenGL(bool isTransparent);
	void buildFacesToDisplay(DisplayData *fillData, DisplayData *transparentFillData);
	void clearFaces();
	void updateSSBO();
	void updateFillData();
};
