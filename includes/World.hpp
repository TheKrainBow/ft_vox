#include "ft_vox.hpp"
#include "NoiseGenerator.hpp"
#include "Chunk.hpp"
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

struct DrawArraysIndirectCommand {
	GLuint count;           // Number of vertices per instance
	GLuint instanceCount;   // Number of instances to draw
	GLuint first;           // Starting index in the vertex buffer
	GLuint baseInstance;    // Starting index in the instance buffer
};

class World
{
private:
	NoiseGenerator															_perlinGenerator;
	std::unordered_map<std::tuple<int, int, int>, std::unique_ptr<Chunk>>	_loadedChunks;
	//std::unordered_map<std::tuple<int, int, int>, std::unique_ptr<Chunk>>	_cachedChunks;
	Camera																	_player;
	GLuint																	_vao;
	GLuint																	_vbo;
	GLuint																	_instanceVBO;
	GLuint																	_indirectBuffer;
	GLuint																	_ssbo;
	std::vector<DrawArraysIndirectCommand>									_indirectCmds;
	std::vector<int>														_vertexData;
	int																		_vertexSize = 0;
public:
	World(int seed);
	~World();
	void loadChunk(vec3 position);
	void loadPerlinMap(vec3 camPosition);
	NoiseGenerator &getNoiseGenerator(void);
	char getBlock(int x, int y, int z);
	int	getLoadedChunksNumber();
	int	getCachedChunksNumber();
	void sendFacesToDisplay();
	Chunk* getChunk(int chunkX, int chunkY, int chunkZ);
	void setupBuffers();
	int display(void);
	int getVertexSize();
	void addVertex(int vertexData);
private:
	vec3 calculateBlockPos(int x, int y, int z) const;
	void findOrLoadChunk(vec3 position, std::unordered_map<std::tuple<int, int, int>, std::unique_ptr<Chunk>>& tempChunks);
};
