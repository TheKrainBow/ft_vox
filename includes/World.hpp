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

class World
{
private:
	// World related informations
		NoiseGenerator								_perlinGenerator;
		std::unordered_map<glm::ivec2, Chunk*, ivec2_hash>	_chunks;
		std::unordered_map<glm::ivec2, Chunk*, ivec2_hash>	_displayedChunks;
		std::list<Chunk *>							_chunkList;
		std::mutex									_chunksListMutex;
		
		std::unordered_map<glm::ivec2, Chunk*, ivec2_hash> _activeChunks;
		std::queue<glm::ivec2>	_chunkRemovalOrder;
		std::queue<Chunk *>	_chunksLoadOrder;
		std::mutex			_chunksRemovalMutex;
		std::mutex			_chunksLoadMutex;
		
		std::mutex									_displayChunkMutex;
		std::queue<Chunk*>							_displayQueue;
		std::mutex									_displayQueueMutex;
		bool										_skipLoad;
		TextureManager								&_textureManager;
		
		// Player related informations
		Camera										*_camera;
		int											_renderDistance;
		int											_maxRender = 1000;
		bool										*_isRunning;
		std::mutex									*_runningMutex;
		std::atomic_bool							displayReady;
		Chrono chronoHelper;
		ThreadPool 									_threadPool;
	public:
		std::mutex									_chunksMutex;
		GLuint 										_shaderProgram;
	World(int seed, TextureManager &textureManager, Camera &camera);
	~World();
	void loadFirstChunks(ivec2 camPosition);
	void init(GLuint shaderProgram, int renderDistance);
	void unLoadNextChunks(ivec2 newCamChunk);
	void loadChunk(int x, int z, int render, ivec2 camPosition, int resolution = 1);
	void loadPerlinMap(vec3 camPosition);
	NoiseGenerator &getNoiseGenerator(void);
	char getBlock(vec3 position);
	int	getCachedChunksNumber();
	Chunk* getChunk(ivec2 position);
	SubChunk* getSubChunk(vec3 position);
	int display();
	void increaseRenderDistance();
	void decreaseRenderDistance();
	int *getRenderDistancePtr();
	void setRunning(std::mutex *runningMutex, bool *isRunning);
private:
	vec3 calculateBlockPos(vec3 position) const;
	bool getIsRunning();
	void loadTopChunks(int render, ivec2 camPosition);
	void loadRightChunks(int render, ivec2 camPosition);
	void loadBotChunks(int render, ivec2 camPosition);
	void loadLeftChunks(int render, ivec2 camPosition);
	void unloadChunk();
	void generateSpiralOrder();
	bool hasMoved(ivec2 oldPos);
	void loadOrder();
	void removeOrder();
	void updateChunk(int x, int z, int render, ivec2 chunkPos);
};
