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
    struct hash<std::tuple<int, int, int>> {
        size_t operator()(const std::tuple<int, int, int>& t) const {
            size_t h1 = std::hash<int>{}(std::get<0>(t));
            size_t h2 = std::hash<int>{}(std::get<1>(t));
            size_t h3 = std::hash<int>{}(std::get<2>(t));
            return h1 ^ (h2 << 1) ^ (h3 << 2);
        }
    };
}

class SubChunk;

class World
{
private:
	// World related informations
		NoiseGenerator                                              _perlinGenerator;
		std::map<std::tuple<int, int, int>, std::unique_ptr<SubChunk>>    _loadedChunks;
		std::map<std::tuple<int, int, int>, std::unique_ptr<SubChunk>>    _cachedChunks;
		Chunk														**_chunks;
	// Player related informations
		Camera														_player;
		int															_renderDistance;
		int															_maxRender = 64;
public:
	World(int seed);
	~World();
	void loadChunk(vec3 camPosition, TextureManager &textManager);
	void loadPerlinMap(vec3 camPosition);
	NoiseGenerator &getNoiseGenerator(void);
	char getBlock(vec3 position);
	int	getLoadedChunksNumber();
	int	getCachedChunksNumber();
	void sendFacesToDisplay();
	SubChunk* getChunk(vec3 position);
	int display(Camera &cam, GLFWwindow *win);
private:
	vec3 calculateBlockPos(vec3 position) const;
	void findOrLoadChunk(vec3 position, std::map<std::tuple<int, int, int>, std::unique_ptr<SubChunk>>& tempChunks, TextureManager &textManager, PerlinMap *perlinMap);
};
