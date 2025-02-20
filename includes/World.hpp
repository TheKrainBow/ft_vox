#pragma once
#include "ft_vox.hpp"
#include "NoiseGenerator.hpp"
#include "SubChunk.hpp"
#include "Chunk.hpp"
#include "Camera.hpp"
#include <map>

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

class SubChunk;

class World
{
private:
	// World related informations
		NoiseGenerator								_perlinGenerator;
		std::map<std::pair<int, int>, Chunk*>		_chunks;
		std::mutex									_chunksMutex;
		std::mutex									_displayMutex;
		Chunk										**_displayedChunk;
		bool										_skipLoad;
		TextureManager								&_textureManager;
	// Player related informations
		Camera										*_camera;
		int											_renderDistance;
		int											_maxRender = 1000;
		bool										*_isRunning;
		std::mutex									*_runningMutex;
public:
	World(int seed, TextureManager &textureManager, Camera &camera);
	~World();
	void loadChunks(vec3 camPosition);
	void loadChunk(int x, int z, int renderMax, int currentRender, vec3 camPosition);
	void loadPerlinMap(vec3 camPosition);
	NoiseGenerator &getNoiseGenerator(void);
	char getBlock(vec3 position);
	int	getLoadedChunksNumber();
	int	getCachedChunksNumber();
	void sendFacesToDisplay();
	SubChunk* getChunk(vec3 position);
	int display();
	void increaseRenderDistance();
	void decreaseRenderDistance();
	int *getRenderDistancePtr();
	void setRunning(std::mutex *runningMutex, bool *isRunning);
private:
	vec3 calculateBlockPos(vec3 position) const;
	bool getIsRunning();
	int loadTopChunks(int renderDistance, int render, vec3 camPosition);
	int loadRightChunks(int renderDistance, int render, vec3 camPosition);
	int loadBotChunks(int renderDistance, int render, vec3 camPosition);
	int loadLeftChunks(int renderDistance, int render, vec3 camPosition);
	void updateNeighbours(std::pair<int, int> pair);
};
