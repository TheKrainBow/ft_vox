#pragma once
#include "ft_vox.hpp"
#include "NoiseGenerator.hpp"
#include "SubChunk.hpp"
#include "Camera.hpp"
#include <unordered_map>

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
	NoiseGenerator                                              _perlinGenerator;
	std::unordered_map<std::tuple<int, int, int>, std::unique_ptr<SubChunk>>    _loadedChunks;
	std::unordered_map<std::tuple<int, int, int>, std::unique_ptr<SubChunk>>    _cachedChunks;
	Camera                                                      _player;
public:
	World(int seed);
	~World();
	void loadChunk(vec3 camPosition, TextureManager &textManager);
	void loadPerlinMap(vec3 camPosition);
	NoiseGenerator &getNoiseGenerator(void);
	char getBlock(int x, int y, int z);
	int	getLoadedChunksNumber();
	int	getCachedChunksNumber();
	void sendFacesToDisplay();
	SubChunk* getChunk(int chunkX, int chunkY, int chunkZ);
	int display(Camera &cam, GLFWwindow *win);
private:
	vec3 calculateBlockPos(int x, int y, int z) const;
	void findOrLoadChunk(vec3 position, std::unordered_map<std::tuple<int, int, int>, std::unique_ptr<SubChunk>>& tempChunks, TextureManager &textManager, NoiseGenerator::PerlinMap *perlinMap);
};
