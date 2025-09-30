#pragma once

#include <unordered_set>
#include <queue>
#include <list>
#include <atomic>
#include <mutex>
#include <unordered_map>
#include <vector>
#include <limits>

#include "ft_vox.hpp"
#include "NoiseGenerator.hpp"
#include "SubChunk.hpp"
#include "CaveGenerator.hpp"
#include "Chunk.hpp"
#include "Camera.hpp"
#include "Chrono.hpp"
#include "ThreadPool.hpp"
#include "Frustum.hpp"
#include "ChunkLoader.hpp"
#include "ChunkRenderer.hpp"
#include "Raycaster.hpp"

class ChunkRenderer
{
private:
	// GL Unsigned ints
	/* Vertex array objects and vertex array buffers
		for solid and transparent rendering */
	GLuint									_vao;
	GLuint									_vbo;
	GLuint									_transparentVao;
	GLuint									_transparentIndirectBuffer;
	GLuint									_indirectBuffer;

	// SSBOs
	GLuint									_solidPosSSBO = 0;      // binding=3 for SOLID
	GLuint									_transpPosSSBO = 0;     // binding=3 for TRANSPARENT
	GLuint									_solidPosSrcSSBO = 0;   // binding 0 for solid pass
	GLuint									_transpPosSrcSSBO = 0;  // binding 0 for transparent pass
	GLuint									_solidInstSSBO = 0;     // binding=4 for SOLID
	GLuint									_transpInstSSBO = 0;    // binding=4 for TRANSPARENT

	// Update notifiers from ChunkManager
	std::atomic_bool						_needUpdate;
	std::atomic_bool						_needTransparentUpdate;

	// === GPU frustum culling ===
	// Discarding chunks outside of frustum view
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
	// Occlusion uniforms
	GLint                                       _locUseOcclu = -1;
	GLint                                       _locDepthTex = -1;
	GLint                                       _locViewport = -1;
	GLint                                       _locView = -1;
	GLint                                       _locProj = -1;
	GLint                                       _locCamPos = -1;

	// Previous-frame depth context for occlusion
	GLuint                                      _occDepthTex = 0;
	int                                         _occW = 0;
	int                                         _occH = 0;
	glm::mat4                                   _occView{1.0f};
	glm::mat4                                   _occProj{1.0f};
	bool                                        _occAvailable = false;
	glm::vec3                                   _occCamPos{0.0f};

		// Debug/metrics
		long long								_lastSolidTris = 0;

		// Last good GPU-cull counts to avoid flashing fallback when mapping fails
		GLsizei									_lastGoodSolidCount = 0;
		GLsizei									_lastGoodTranspCount = 0;

	// Buffer capacities to minimize reallocations
	GLsizeiptr								_capTemplSolidCmd  = 0;
	GLsizeiptr								_capOutSolidCmd    = 0;
	GLsizeiptr								_capSolidInst      = 0;
	GLsizeiptr								_capSolidSSBO      = 0;
	GLsizeiptr								_capTemplTranspCmd = 0;
	GLsizeiptr								_capOutTranspCmd   = 0;
	GLsizeiptr								_capTranspInst     = 0;
	GLsizeiptr								_capTranspSSBO     = 0;
	GLsizeiptr								_capSolidSSBOSrc  = 0;
	GLsizeiptr								_capTranspSSBOSrc = 0;

	// Common mutexes between ChunkLoader and ChunkRenderer
	std::mutex								&_solidDrawDataMutex;
	std::mutex								&_transparentDrawDataMutex;

	// Solid and transparent rendering data
	DisplayData								*_solidDrawData;
	DisplayData								*_transparentDrawData;

	// Staged rendering data queue shared with ChunkLoader
	std::queue<DisplayData *>	&_solidStagedDataQueue;
	std::queue<DisplayData *>	&_transparentStagedDataQueue;

	// Boolean to avoid double buffer init
	bool	_hasBufferInitialized;

	// Optional GPU sync after each solid draw (used for shadow cascades)
	bool    _syncAfterDraw = false;
public:
	ChunkRenderer(
		std::mutex &solidDrawDataMutex,
		std::mutex &transparentDrawDataMutex,
		std::queue<DisplayData *>	&solidStagedDataQueue,
		std::queue<DisplayData *>	&transparentStagedDataQueue
	);
	~ChunkRenderer();

	// Rendering methods
	int renderSolidBlocks();
	int renderTransparentBlocks();
	// Shadow pass helper: draw transparent terrain (leaves) without GPU culling
	// Uses template indirect commands and source SSBO (no compute compaction)
	int renderTransparentBlocksNoCullForShadow();

	// OpenGL setup for rendering
	void initGLBuffer();

	// Explicit GL teardown
	void shutdownGL();

	// Shared data setters
	void setViewProj(glm::vec4 planes[6]);
	void setOcclusionSource(GLuint depthTex, int width, int height,
							const glm::mat4& view, const glm::mat4& proj,
							const glm::vec3& camPos);

	// Swap current rendering data with new one sent from ChunkLoader
	void updateDrawData();

	// Control optional GPU sync after draw (avoid buffer races across passes)
	void setSyncAfterDraw(bool enabled) { _syncAfterDraw = enabled; }
private:
	// GPU side frustum culling helpers
	void initGpuCulling();
	void runGpuCulling(bool transparent);

	// Helper to send vertices to GPU before rendering stage
	// Both solid and transparent
	void pushVerticesToOpenGL(bool isTransparent);
};
