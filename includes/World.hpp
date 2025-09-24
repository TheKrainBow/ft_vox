#pragma once
#include "ft_vox.hpp"
#include "NoiseGenerator.hpp"
#include "SubChunk.hpp"
#include "CaveGenerator.hpp"
#include "Chunk.hpp"
#include "Camera.hpp"
#include "Chrono.hpp"
#include "ThreadPool.hpp"
#include "Frustum.hpp"

#include <unordered_set>
#include <queue>
#include <list>
#include <atomic>
#include <mutex>
#include <unordered_map>
#include <vector>
#include <limits>

class Chunk;
class SubChunk;

class World {
private:
	// Chunk loading utils
	ThreadPool     &_threadPool;

	// Player related informations
	Camera         &_camera;
	Chrono          _chronoHelper;

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
	GLuint									_solidPosSrcSSBO = 0;   // binding 0 for solid pass
	GLuint									_transpPosSrcSSBO = 0;  // binding 0 for transparent pass
	GLuint									_solidInstSSBO = 0;     // binding=4 for SOLID
	GLuint									_transpInstSSBO = 0;    // binding=4 for TRANSPARENT

	bool									_needUpdate;
	bool									_needTransparentUpdate;

	// === GPU frustum culling ===
	GLuint									_cullProgram = 0;
	GLuint									_frustumUBO  = 0;
	GLuint									_templIndirectBuffer = 0;
	GLuint									_transpTemplIndirectBuffer = 0;
	GLuint									_solidParamsBuf = 0;
	GLuint									_transpParamsBuf = 0;

	GLsizei									_solidDrawCount = 0;
	GLsizei									_transpDrawCount = 0;
	GLint									_locNumDraws = -1;
	GLint									_locChunkSize = -1;

	// Debug/metrics (avoid re-reading CPU buffers after upload)
	long long								_lastSolidTris = 0;

	// Buffer capacities to minimize reallocations (amortize uploads)
	GLsizeiptr								_capTemplSolidCmd  = 0;
	GLsizeiptr								_capOutSolidCmd    = 0;
	GLsizeiptr								_capSolidInst      = 0;
	GLsizeiptr								_capSolidSSBO      = 0;
	GLsizeiptr								_capTemplTranspCmd = 0;
	GLsizeiptr								_capOutTranspCmd   = 0;
	GLsizeiptr								_capTranspInst     = 0;
	GLsizeiptr								_capTranspSSBO     = 0;
	// --- NEW caps for the source pos buffers ---
	GLsizeiptr								_capSolidSSBOSrc  = 0;
	GLsizeiptr								_capTranspSSBOSrc = 0;

	// Concurrency guard to avoid concurrent/stacked heavy builds
	std::atomic_bool						_buildingDisplay = false;
	std::atomic_bool *_isRunning      = nullptr;
public:
	// Init
	World(int seed, Camera &camera, ThreadPool &pool, std::atomic_bool *isRunning);
	~World();

	// Display
	int displaySolid();
	int displayTransparent();

	// Runtime update
	void updateDrawData();

	// Shared data getters
	void setViewProj(const glm::mat4& view, const glm::mat4& proj);
	void getDisplayedChunksSnapshot(std::vector<glm::ivec2>& out);

	// Explicit GL teardown (call before destroying the GL context)
	void shutdownGL();

	// TODO: Create raycaster class
	bool raycastHit(const glm::vec3& originWorld,
		const glm::vec3& dirWorld,
		float maxDistance,
		glm::ivec3& outBlock);
	BlockType raycastHitFetch(const glm::vec3& originWorld,
		const glm::vec3& dirWorld,
		float maxDistance,
		glm::ivec3& outBlock);
	bool raycastDeleteOne(const glm::vec3& originWorld,
		const glm::vec3& dirWorld,
		float maxDistance = 5.0f);
	bool raycastPlaceOne(const glm::vec3& originWorld,
				const glm::vec3& dirWorld,
				float maxDistance,
				BlockType block);
	bool getIsRunning();
private:
	// Chunk loading
	void scheduleDisplayUpdate();

	// Runtime info
	void initGpuCulling();
	void runGpuCulling(bool transparent);

	// Display
	void initGLBuffer();
	void pushVerticesToOpenGL(bool isTransparent);
};
