#pragma once
#include "ft_vox.hpp"
#include "NoiseGenerator.hpp"
#include "SubChunk.hpp"
#include "CaveGenerator.hpp"
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

struct CmdRange { uint32_t start; uint32_t count; };
struct AABB { vec3 mn, mx; };

struct DisplayData
{
	std::vector<vec4>						ssboData;
	std::vector<int>						vertexData;
	std::vector<DrawArraysIndirectCommand>	indirectBufferData;

	std::unordered_map<ivec2, CmdRange, ivec2_hash> chunkCmdRanges;

	std::vector<AABB> cmdAABBsSolid;
	std::vector<AABB> cmdAABBsTransp;
};

struct FrustumPlane { glm::vec3 n; float d; };
struct Frustum {
	FrustumPlane p[6]; // L,R,B,T,N,F
	static Frustum fromVP(const glm::mat4& VP) {
		Frustum f{}; const glm::mat4& m = VP; auto norm = [](FrustumPlane& pl){
			float inv = 1.0f / glm::length(pl.n); pl.n *= inv; pl.d *= inv;
		};
		// column-major: row i is (m[0][i], m[1][i], m[2][i], m[3][i])
		auto row = [&](int i){ return glm::vec4(m[0][i], m[1][i], m[2][i], m[3][i]); };
		glm::vec4 r0=row(0), r1=row(1), r2=row(2), r3=row(3);

		auto set = [&](FrustumPlane& pl, const glm::vec4& v){
			pl.n = glm::vec3(v.x, v.y, v.z); pl.d = v.w; norm(pl);
		};
		set(f.p[0], r3 + r0); // Left
		set(f.p[1], r3 - r0); // Right
		set(f.p[2], r3 + r1); // Bottom
		set(f.p[3], r3 - r1); // Top
		set(f.p[4], r3 + r2); // Near
		set(f.p[5], r3 - r2); // Far
		return f;
	}
	bool aabbVisible(const glm::vec3& mn, const glm::vec3& mx) const {
		glm::vec3 c = (mn + mx) * 0.5f;
		glm::vec3 e = (mx - mn) * 0.5f;
		for (int i=0;i<6;++i) {
			const auto& pl = p[i];
			float r = e.x * std::abs(pl.n.x) + e.y * std::abs(pl.n.y) + e.z * std::abs(pl.n.z);
			float s = glm::dot(pl.n, c) + pl.d;
			// Outside the plane
			if (s + r < 0.0f) return false;
		}
		return true;
	}
};

class World
{
private:
	// Chunk loading info
	std::queue<DisplayData *>	_stagedDataQueue;
	std::queue<DisplayData *>	_transparentStagedDataQueue;
	std::list<Chunk *>							_chunkList;
	std::mutex									_chunksListMutex;
	size_t										_memorySize;
	std::unordered_map<ivec2, Chunk*, ivec2_hash>	_chunks;
	std::unordered_map<ivec2, Chunk*, ivec2_hash>	_displayedChunks;
	std::mutex										_displayedChunksMutex;

	// Chunk loading utils
	ThreadPool 									&_threadPool;
	TextureManager								&_textureManager;

	// Player related informations
	Camera										&_camera;
	int											_renderDistance;
	int											_currentRender;
	int											_maxRender = 1000;
	bool										*_isRunning;
	std::mutex									*_runningMutex;
	Chrono										_chronoHelper;


	// Display / Runtime data
	std::mutex								_drawDataMutex;
	DisplayData								*_drawData;
	bool 									_hasBufferInitialized;

	DisplayData								*_transparentDrawData;

	GLuint									_indirectBuffer;
	GLuint									_vao;
	GLuint									_vbo;

	GLuint									_transparentVao;
	GLuint									_transparentIndirectBuffer;

	// Single-buffer (no triple PB) SSBOs per pass
	GLuint									_solidPosSSBO = 0;      // binding=3 for SOLID
	GLuint									_transpPosSSBO = 0;     // binding=3 for TRANSPARENT
	GLuint									_solidInstSSBO = 0;     // binding=4 for SOLID
	GLuint									_transpInstSSBO = 0;    // binding=4 for TRANSPARENT

	bool									_needUpdate;
	bool									_needTransparentUpdate;
	std::atomic_int 						_threshold;
	NoiseGenerator							_perlinGenerator;
	std::mutex								_chunksMutex;

	CaveGenerator							_caveGen;
	// === GPU frustum culling ===
	GLuint									_cullProgram = 0;
	GLuint									_frustumUBO  = 0;
	GLuint									_templIndirectBuffer = 0;
	GLuint									_transpTemplIndirectBuffer = 0;
	GLsizei									_solidDrawCount = 0;
	GLsizei									_transpDrawCount = 0;
	GLint									_locNumDraws = -1;
	GLint									_locChunkSize = -1;



public:
	// Init
	World(int seed, TextureManager &textureManager, Camera &camera, ThreadPool &pool);
	~World();
	void init(int renderDistance);
	void setRunning(std::mutex *runningMutex, bool *isRunning);
	
	// Display
	int display();
	int displayTransparent();

	// Runtime update
	void updatePerlinMapResolution(PerlinMap *pMap, int newResolution);
	void updateDrawData();
	
	// Runtime chunk loading/unloading
	void loadFirstChunks(ivec2 &camPosition);
	void unLoadNextChunks(ivec2 &newCamChunk);
	
	// Shared data getters
	NoiseGenerator &getNoiseGenerator(void);
	Chunk* getChunk(ivec2 &position);
	SubChunk* getSubChunk(ivec3 &position);
	size_t *getMemorySizePtr();
	int *getRenderDistancePtr();
	int *getCurrentRenderPtr();
	TopBlock findTopBlockY(ivec2 chunkPos, ivec2 worldPos);
	TopBlock findBlockUnderPlayer(ivec2 chunkPos, ivec3 worldPos);
	BlockType getBlock(ivec2 chunkPos, ivec3 worldPos);
	void setViewProj(const glm::mat4& view, const glm::mat4& proj);
	void endFrame();
	// Explicit GL teardown (call before destroying the GL context)
	void shutdownGL();
private:
	// Chunk loading
	Chunk *loadChunk(int x, int z, int render, ivec2 &chunkPos, int resolution);
	void loadTopChunks(int render, ivec2 &camPosition, int resolution = 1);
	void loadRightChunks(int render, ivec2 &camPosition, int resolution = 1);
	void loadBotChunks(int render, ivec2 &camPosition, int resolution = 1);
	void loadLeftChunks(int render, ivec2 &camPosition, int resolution = 1);
	void unloadChunk();
	void initBigSSBO(GLsizeiptr bytes);

	// Runtime info
	bool hasMoved(ivec2 &oldPos);
	bool getIsRunning();
	void initGpuCulling();
	void runGpuCulling(bool transparent);


	// Display
	void initGLBuffer();
	void pushVerticesToOpenGL(bool isTransparent);
	void buildFacesToDisplay(DisplayData *fillData, DisplayData *transparentFillData);
	void updateFillData();
};
