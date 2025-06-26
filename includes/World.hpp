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
	// World related informations
	std::unordered_map<ivec2, Chunk*, ivec2_hash>	_chunks;
	std::unordered_map<ivec2, Chunk*, ivec2_hash>	_displayedChunks;
	std::mutex _displayedChunksMutex;
	std::list<Chunk *>							_chunkList;
	std::mutex									_chunksListMutex;
	size_t										_memorySize;
	
	std::unordered_map<ivec2, Chunk*, ivec2_hash> _activeChunks;
	
	
	std::queue<DisplayData *>	_stagedDataQueue;
	std::queue<DisplayData *>	_transparentStagedDataQueue;
	
	bool										_skipLoad;
	ThreadPool 									&_threadPool;
	TextureManager								&_textureManager;
	
	// Player related informations
	Camera										&_camera;
	int											_renderDistance;
	int											_currentRender;
	int											_maxRender = 1000;
	bool										*_isRunning;
	std::mutex									*_runningMutex;
	std::atomic_bool							displayReady;
	Chrono										chronoHelper;

	bool 									_hasBufferInitialized;
	GLuint									_ssbo;

	// Display
	std::mutex								_drawDataMutex;
	DisplayData								*_drawData;

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
	NoiseGenerator								_perlinGenerator;
	GLuint 										_shaderProgram;
	std::mutex									_chunksMutex;
public:
	World(int seed, TextureManager &textureManager, Camera &camera, ThreadPool &pool);
	~World();
	
	void init(GLuint shaderProgram, int renderDistance);
	void loadFirstChunks(ivec2 &camPosition);
	void unLoadNextChunks(ivec2 &newCamChunk);
	void loadPerlinMap(ivec3 &camPosition);
	NoiseGenerator &getNoiseGenerator(void);
	int	getCachedChunksNumber();
	Chunk* getChunk(ivec2 &position);
	SubChunk* getSubChunk(ivec3 &position);
	int display();
	int displayTransparent();
	void increaseRenderDistance();
	void decreaseRenderDistance();
	int *getRenderDistancePtr();
	int *getCurrentRenderPtr();
	size_t *getMemorySizePtr();
	void setRunning(std::mutex *runningMutex, bool *isRunning);
	void updateDrawData();
	void updatePerlinMapResolution(PerlinMap *pMap, int newResolution);
private:
	bool getIsRunning();
	void loadChunk(int x, int z, int render, ivec2 &chunkPos, int resolution);
	void loadTopChunks(int render, ivec2 &camPosition, int resolution = 1);
	void loadRightChunks(int render, ivec2 &camPosition, int resolution = 1);
	void loadBotChunks(int render, ivec2 &camPosition, int resolution = 1);
	void loadLeftChunks(int render, ivec2 &camPosition, int resolution = 1);
	void unloadChunk();
	bool hasMoved(ivec2 &oldPos);

	void pushVerticesToOpenGL(bool isTransparent);
	void clearFaces();
	void buildFacesToDisplay(DisplayData *fillData, DisplayData *transparentFillData);
	void updateSSBO();
	void updateFillData();
	void initGLBuffer();
};
