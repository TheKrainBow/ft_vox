#include "ft_vox.hpp"
#include "NoiseGenerator.hpp"
#include "ChunkV2.hpp"
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
    std::unordered_map<std::tuple<int, int, int>, ChunkV2 *>    _loadedChunks;
    std::unordered_map<std::tuple<int, int, int>, ChunkV2 *>    _cachedChunks;
    Camera                                                      _player;
public:
    World(int seed);
    ~World();
    void loadChunk(vec3 position, int renderDistance);
    NoiseGenerator &getNoiseGenerator(void);
    BlockType getBlock(int x, int y, int z);
    void sendFacesToDisplay();
};
