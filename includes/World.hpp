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
		NoiseGenerator												_perlinGenerator;
		std::map<std::pair<int, int>, Chunk*>						_chunks;
		Chunk														**_displayedChunk;
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
};
