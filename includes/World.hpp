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
		std::unordered_map<std::pair<int, int>, Chunk*, pair_hash>	_chunks;
		std::list<Chunk *>							_chunkList;
		std::mutex									_chunksMutex;
		std::mutex									_chunksListMutex;
		ChunkSlot									*_displayedChunk;
		std::mutex									_displayChunkMutex;
		std::queue<Chunk*>							_displayQueue;
		std::mutex									_displayQueueMutex;
		bool										_skipLoad;
		TextureManager								&_textureManager;
		std::vector<std::pair<int, int>>			_spiralOrder;
		
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
	World(int seed, TextureManager &textureManager, Camera &camera);
	~World();
	void loadChunks(vec3 camPosition);
	void loadChunk(int x, int z, int renderMax, int currentRender, vec3 camPosition);
	void loadPerlinMap(vec3 camPosition);
	NoiseGenerator &getNoiseGenerator(void);
	char getBlock(vec3 position);
	int	getCachedChunksNumber();
	Chunk* getChunk(vec2 position);
	SubChunk* getSubChunk(vec3 position);
	int display();
	void increaseRenderDistance();
	void decreaseRenderDistance();
	int *getRenderDistancePtr();
	void setRunning(std::mutex *runningMutex, bool *isRunning);
private:
	vec3 calculateBlockPos(vec3 position) const;
	bool getIsRunning();
	void loadTopChunks(int renderDistance, int render, vec3 camPosition);
	void loadRightChunks(int renderDistance, int render, vec3 camPosition);
	void loadBotChunks(int renderDistance, int render, vec3 camPosition);
	void loadLeftChunks(int renderDistance, int render, vec3 camPosition);
	void updateNeighbours(std::pair<int, int> pair);
	void unloadChunk();
	void generateSpiralOrder();
	void resetTerrain();
	bool hasMoved(vec3 oldPos);
};
