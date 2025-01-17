#include "ft_vox.hpp"
#include "NoiseGenerator.hpp"
#include "Chunk.hpp"
#include "Camera.hpp"
namespace std {
    template <>
    struct hash<std::tuple<int, int, int>> {
        size_t operator()(const std::tuple<int, int, int>& t) const {
            size_t h1 = std::hash<int>{}(std::get<0>(t));
            size_t h2 = std::hash<int>{}(std::get<1>(t));
            size_t h3 = std::hash<int>{}(std::get<2>(t));
            // std::cout << "Hash for key (" << std::get<0>(t) << ", "
            //           << std::get<1>(t) << ", " << std::get<2>(t) << "): "
            //           << (h1 ^ (h2 << 1) ^ (h3 << 2)) << std::endl;
            return h1 ^ (h2 << 1) ^ (h3 << 2);
        }
    };
}

class World
{
private:
	NoiseGenerator                                              _perlinGenerator;
	std::unordered_map<std::tuple<int, int, int>, std::unique_ptr<Chunk>>    _loadedChunks;
	std::unordered_map<std::tuple<int, int, int>, std::unique_ptr<Chunk>>    _cachedChunks;
	Camera                                                      _player;
public:
	World(int seed);
	~World();
	void loadChunk(vec3 position);
	void loadPerlinMap(vec3 camPosition);
	NoiseGenerator &getNoiseGenerator(void);
	char getBlock(int x, int y, int z);
	void sendFacesToDisplay();
	Chunk* getChunk(int chunkX, int chunkY, int chunkZ);
private:
	vec3 calculateBlockPos(int x, int y, int z) const;
	void findOrLoadChunk(vec3 position, std::unordered_map<std::tuple<int, int, int>, std::unique_ptr<Chunk>>& tempChunks);
};
