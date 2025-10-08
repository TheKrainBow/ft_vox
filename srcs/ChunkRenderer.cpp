#include "ChunkRenderer.hpp"
#include "define.hpp"

ChunkRenderer::ChunkRenderer(
	std::mutex &solidDrawDataMutex,
	std::mutex &transparentDrawDataMutex,
	std::queue<DisplayData *>	&solidStagedDataQueue,
	std::queue<DisplayData *>	&transparentStagedDataQueue
) :
_needUpdate(true),
_needTransparentUpdate(true),
_solidDrawDataMutex(solidDrawDataMutex),
_transparentDrawDataMutex(transparentDrawDataMutex),
_solidDrawData(nullptr),
_transparentDrawData(nullptr),
_solidStagedDataQueue(solidStagedDataQueue),
_transparentStagedDataQueue(transparentStagedDataQueue),
_hasBufferInitialized(false)
{
	// Disable heavy Hi-Z debug logging by default (prevents frame stalls)
	_enableHiZDebugLogs = false;
}

ChunkRenderer::~ChunkRenderer()
{
	delete _solidDrawData;
	delete _transparentDrawData;
	if (_uploadGuard) { glDeleteSync(_uploadGuard); _uploadGuard = 0; }
}

// Explicit GL teardown
void ChunkRenderer::shutdownGL()
{
	// Delete GL programs and buffers created by ChunkRenderer
	if (_cullProgram) { glDeleteProgram(_cullProgram); _cullProgram = 0; }
	if (_vao) { glDeleteVertexArrays(1, &_vao); _vao = 0; }
	if (_transparentVao) { glDeleteVertexArrays(1, &_transparentVao); _transparentVao = 0; }

	if (_vbo) { glDeleteBuffers(1, &_vbo); _vbo = 0; }
	if (_indirectBuffer) { glDeleteBuffers(1, &_indirectBuffer); _indirectBuffer = 0; }

	if (_transparentIndirectBuffer) { glDeleteBuffers(1, &_transparentIndirectBuffer); _transparentIndirectBuffer = 0; }

	if (_templIndirectBuffer) { glDeleteBuffers(1, &_templIndirectBuffer); _templIndirectBuffer = 0; }
	if (_transpTemplIndirectBuffer) { glDeleteBuffers(1, &_transpTemplIndirectBuffer); _transpTemplIndirectBuffer = 0; }
	if (_frustumUBO) { glDeleteBuffers(1, &_frustumUBO); _frustumUBO = 0; }

	if (_solidPosSSBO) { glDeleteBuffers(1, &_solidPosSSBO); _solidPosSSBO = 0; }
	if (_transpPosSSBO) { glDeleteBuffers(1, &_transpPosSSBO); _transpPosSSBO = 0; }
	if (_solidInstSSBO) { glDeleteBuffers(1, &_solidInstSSBO); _solidInstSSBO = 0; }
	if (_transpInstSSBO) { glDeleteBuffers(1, &_transpInstSSBO); _transpInstSSBO = 0; }
	if (_solidPosSrcSSBO)  { glDeleteBuffers(1, &_solidPosSrcSSBO);  _solidPosSrcSSBO = 0; }
	if (_transpPosSrcSSBO) { glDeleteBuffers(1, &_transpPosSrcSSBO); _transpPosSrcSSBO = 0; }
	if (_solidMetaSrcSSBO) { glDeleteBuffers(1, &_solidMetaSrcSSBO); _solidMetaSrcSSBO = 0; }
	if (_transpMetaSrcSSBO){ glDeleteBuffers(1, &_transpMetaSrcSSBO); _transpMetaSrcSSBO = 0; }
	if (_solidMetaSSBO)    { glDeleteBuffers(1, &_solidMetaSSBO);    _solidMetaSSBO = 0; }
	if (_transpMetaSSBO)   { glDeleteBuffers(1, &_transpMetaSSBO);   _transpMetaSSBO = 0; }

	// Debug Hi-Z buffers
	if (_cullDebugIDsSSBO)   { glDeleteBuffers(1, &_cullDebugIDsSSBO);   _cullDebugIDsSSBO = 0; }
	if (_cullDebugCountBuf)  { glDeleteBuffers(1, &_cullDebugCountBuf);  _cullDebugCountBuf = 0; }

	// Hysteresis buffer
	if (_hystSSBO) { glDeleteBuffers(1, &_hystSSBO); _hystSSBO = 0; }

	// Reveal buffer
	if (_revealSSBO) { glDeleteBuffers(1, &_revealSSBO); _revealSSBO = 0; }
}

// Shared data setters
void ChunkRenderer::setViewProj(glm::vec4 planes[6]) {
	// Orphan UBO storage to avoid writing into memory potentially in use
	glNamedBufferData(_frustumUBO, 6 * sizeof(glm::vec4), planes, GL_DYNAMIC_DRAW);
	// Reset occlusion source unless explicitly set for this view
	_occAvailable = false;
}

void ChunkRenderer::setOcclusionSource(GLuint depthTex, int width, int height,
									const glm::mat4& view, const glm::mat4& proj,
									const glm::vec3& camPos)
{
	// If draw data changed this frame, previous-frame depth may be invalid.
	// In that case, disable occlusion for this frame to avoid popping.
	// Do not gate solid-pass occlusion on transparent updates; the solid
	// pass can safely use previous-frame depth even if transparent buffers
	// haven’t been refreshed yet.
	bool disableOcclusionThisFrame = _needUpdate.load();

	_occDepthTex = depthTex;
	_occW = width;
	_occH = height;
	_occView = view;
	_occProj = proj;
	_occCamPos = camPos;
	_occAvailable = (!disableOcclusionThisFrame && _occDepthTex != 0 && _occW > 0 && _occH > 0);
}

// Swap old rendering data with new staged data from ChunkLoader
void ChunkRenderer::updateDrawData()
{
	std::lock_guard<std::mutex> lock(_solidDrawDataMutex);

	// queues: keep only the newest staged data to minimize uploads
	while (_solidStagedDataQueue.size() > 1)
	{
		DisplayData *old = _solidStagedDataQueue.front();
		_solidStagedDataQueue.pop();
		delete old;
	}
	while (_transparentStagedDataQueue.size() > 1)
	{
		DisplayData *old = _transparentStagedDataQueue.front();
		_transparentStagedDataQueue.pop();
		delete old;
	}
	if (!_solidStagedDataQueue.empty())
	{
		DisplayData *stagedData = _solidStagedDataQueue.front();
		std::swap(stagedData, _solidDrawData);
		delete stagedData;
		stagedData = nullptr;
		_solidStagedDataQueue.pop();
		_needUpdate = true;
		// Avoid warmup template path to prevent initial full-scene flash
		_solidWarmupFrames = 0;
	}
	if (!_transparentStagedDataQueue.empty())
	{
		
		DisplayData *stagedData = _transparentStagedDataQueue.front();
		std::swap(stagedData, _transparentDrawData);
		delete stagedData;
		stagedData = nullptr;
		_transparentStagedDataQueue.pop();
		_needTransparentUpdate = true;
		// Avoid warmup template path for transparent as well
		_transpWarmupFrames = 0;
	}

	// SSBOs are uploaded per-pass in pushVerticesToOpenGL
	// removed debug mismatch logs for solid
	// Transparent sanity (uses the same SSBO)
	// removed debug mismatch logs for transparent
}

// Render passes for solid and transparent blocks
int ChunkRenderer::renderSolidBlocks()
{
	if (!_solidDrawData) return 0;
	if (_needUpdate) { pushVerticesToOpenGL(false); }
	if (_solidDrawCount == 0) {
		// No fresh commands this frame. If a last good count exists, draw those
		// existing commands to avoid an empty frame while chunks update.
		if (_lastGoodSolidCount > 0) {
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, _solidPosSSBO);
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, _solidInstSSBO);
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 7, _solidMetaSSBO);
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 7, _solidMetaSSBO);
			glBindVertexArray(_vao);
			glBindBuffer(GL_DRAW_INDIRECT_BUFFER, _indirectBuffer);
			glBindBuffer(GL_PARAMETER_BUFFER_ARB, 0);
			glMultiDrawArraysIndirect(
				GL_TRIANGLE_STRIP, nullptr, _lastGoodSolidCount,
				sizeof(DrawArraysIndirectCommand)
			);
			glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
			glBindVertexArray(0);
			return (int)_lastSolidTris; // approximate, kept from last build
		}
		return 0;
	}

	// Warmup/template path intentionally disabled on startup to avoid flashing
	bool useTemplatePath = (_solidWarmupFrames > 0);
	if (_solidDrawCount > 0 && _solidPosSrcSSBO && _templIndirectBuffer && useTemplatePath) {
		// Template path: bind SOURCE position, instances and per-draw meta
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, _solidPosSrcSSBO);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, _solidInstSSBO);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 7, _solidMetaSrcSSBO);
		glBindVertexArray(_vao);
		glBindBuffer(GL_DRAW_INDIRECT_BUFFER, _templIndirectBuffer);
		glBindBuffer(GL_PARAMETER_BUFFER_ARB, 0);
		glMultiDrawArraysIndirect(GL_TRIANGLE_STRIP, nullptr, _solidDrawCount,
								   sizeof(DrawArraysIndirectCommand));
		glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
		glBindVertexArray(0);
		if (_solidWarmupFrames > 0) --_solidWarmupFrames;
		return (int)_lastSolidTris;
	}

	runGpuCulling(false);

	// Bind per-pass SSBOs (single-buffer path)
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, _solidPosSSBO);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, _solidInstSSBO);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 7, _solidMetaSSBO);

	glBindVertexArray(_vao);
	glBindBuffer(GL_DRAW_INDIRECT_BUFFER, _indirectBuffer);
	glBindBuffer(GL_PARAMETER_BUFFER_ARB, _solidParamsBuf); // contains uint drawCount
	// removed dump diagnostics for solid
	// Defensive sanity: if the first few compacted commands are zero but a draw count exists,
	// previously we fell back to using template commands (draw-all). That causes visible flashes.
	// Only attempt a fallback when we have a non-zero last good count; otherwise, skip.
	if (_solidDrawCount > 0 && _lastGoodSolidCount > 0) {
		const size_t probe = std::min<size_t>(8, (size_t)_solidDrawCount);
		std::vector<DrawArraysIndirectCommand> head(probe);
		glGetNamedBufferSubData(_indirectBuffer, 0, probe * sizeof(DrawArraysIndirectCommand), head.data());
		bool allZero = true;
		for (size_t i = 0; i < probe; ++i) {
			if (head[i].instanceCount != 0u) { allZero = false; break; }
		}
		if (allZero) {
			// Skip copying full template to avoid draw-all; rely on last good count instead.
			// No action here; the safety block below will use _lastGoodSolidCount.
		}
	}

	glMultiDrawArraysIndirectCount(GL_TRIANGLE_STRIP, /*indirect*/ nullptr,
								   /*drawcount*/ 0, /*maxcount*/ _solidDrawCount,
								   sizeof(DrawArraysIndirectCommand));

	// Robust path for small draw lists: avoid IndirectCount on some drivers
	// that may drop draws when the count is small. Read the count and use
	// glMultiDrawArraysIndirect explicitly.
	bool usedExplicit = false;
	if (_solidDrawCount <= 32) {
		GLuint* mapped = (GLuint*)glMapNamedBufferRange(
			_solidParamsBuf, 0, sizeof(GLuint), GL_MAP_READ_BIT);
		GLuint dc = 0;
		if (mapped) { dc = *mapped; glUnmapNamedBuffer(_solidParamsBuf); }
		if (dc > 0) {
			_lastGoodSolidCount = (GLsizei)dc;
			glBindBuffer(GL_PARAMETER_BUFFER_ARB, 0);
			glMultiDrawArraysIndirect(GL_TRIANGLE_STRIP, nullptr, (GLsizei)dc,
									   sizeof(DrawArraysIndirectCommand));
			glBindBuffer(GL_PARAMETER_BUFFER_ARB, _solidParamsBuf);
			usedExplicit = true;
		}
	}
	if (!usedExplicit) {
		glMultiDrawArraysIndirectCount(GL_TRIANGLE_STRIP, /*indirect*/ nullptr,
									   /*drawcount*/ 0, /*maxcount*/ _solidDrawCount,
									   sizeof(DrawArraysIndirectCommand));
	}

	// Safety fallback: if GPU culling produced zero draws OR mapping failed,
	// prefer to reuse last good count; do NOT fall back to drawing all commands.
	if (!usedExplicit) {
		bool needFallback = false;
		bool zeroCount    = false;
		GLuint* mapped = (GLuint*)glMapNamedBufferRange(
			_solidParamsBuf, 0, sizeof(GLuint), GL_MAP_READ_BIT);
		if (mapped) {
			GLuint dc = *mapped;
			glUnmapNamedBuffer(_solidParamsBuf);
			zeroCount = (dc == 0u);
			if (!zeroCount) {
				_lastGoodSolidCount = (GLsizei)dc;
			} else {
				needFallback = true;
			}
		} else {
			needFallback = true;
		}
		if (needFallback) {
			// Only draw when we have a prior valid count; otherwise, skip to avoid flashing.
			GLsizei count = _lastGoodSolidCount;
			if (count > 0) {
				glBindBuffer(GL_PARAMETER_BUFFER_ARB, 0);
				glMultiDrawArraysIndirect(GL_TRIANGLE_STRIP, nullptr, count,
										   sizeof(DrawArraysIndirectCommand));
				glBindBuffer(GL_PARAMETER_BUFFER_ARB, _solidParamsBuf);
				if (_lastGoodSolidCount == 0) _lastGoodSolidCount = count;
			}
		}
	}
	glBindBuffer(GL_PARAMETER_BUFFER_ARB, 0);
	glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);

	// When requested (e.g., during shadow-cascade rendering), insert a GPU fence
	// so subsequent compute passes won’t overwrite shared buffers while this draw
	// is still in flight. This avoids rare “scrambled subchunk” flashes.
	if (_syncAfterDraw) {
		GLsync fence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
		if (fence) {
			// Wait until the GPU reaches the fence; flush if needed.
			(void)glClientWaitSync(fence, GL_SYNC_FLUSH_COMMANDS_BIT, GLuint64(1000000000)); // 1s cap
			glDeleteSync(fence);
		}
	}

	// Always set an upload guard after finishing a draw using these buffers.
	// On the next upload, wait on this to avoid races.
	if (_uploadGuard) { glDeleteSync(_uploadGuard); _uploadGuard = 0; }
	_uploadGuard = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);

	// Prefer cached triangles count if CPU buffers were discarded
	long long tris = 0;
	if (!_solidDrawData->indirectBufferData.empty()) {
		for (const auto& c : _solidDrawData->indirectBufferData) {
			long long per = (c.count >= 3) ? (c.count - 2) : 0;
			tris += (long long)c.instanceCount * per;
		}
		_lastSolidTris = tris;
	} else {
		tris = _lastSolidTris;
	}

	// After solid upload/draw, CPU-side solid draw data is no longer needed
	// Keep ssboData for the transparent pass upload
	if (!_needUpdate) {
		_solidDrawData->vertexData.clear();
		_solidDrawData->indirectBufferData.clear();
		_solidDrawData->drawMeta.clear();
		std::vector<int>().swap(_solidDrawData->vertexData);
		std::vector<DrawArraysIndirectCommand>().swap(_solidDrawData->indirectBufferData);
		std::vector<uint32_t>().swap(_solidDrawData->drawMeta);
	}
	// removed draw count log
	return (int)tris;
}

int ChunkRenderer::renderTransparentBlocks()
{
	if (!_transparentDrawData) return 0;
	if (_needTransparentUpdate) { pushVerticesToOpenGL(true); }
	if (_transpDrawCount == 0) return 0;

	// Bypass compute compaction for transparent: use template commands and SOURCE buffers
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, _transpPosSrcSSBO);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, _transpInstSSBO);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 7, _transpMetaSrcSSBO);

	glDisable(GL_CULL_FACE);
	glBindVertexArray(_transparentVao);
	glBindBuffer(GL_DRAW_INDIRECT_BUFFER, _transpTemplIndirectBuffer);
	glMultiDrawArraysIndirect(GL_TRIANGLE_STRIP, nullptr, _transpDrawCount,
							  sizeof(DrawArraysIndirectCommand));
	glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
	glBindVertexArray(0);
	// Set an upload guard after finishing transparent draw, too
	if (_uploadGuard) { glDeleteSync(_uploadGuard); _uploadGuard = 0; }
	_uploadGuard = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
	// After transparent upload/draw, CPU-side transparent draw data is no longer needed
	if (!_needTransparentUpdate) {
		_transparentDrawData->vertexData.clear();
		_transparentDrawData->indirectBufferData.clear();
		_transparentDrawData->drawMeta.clear();
		std::vector<int>().swap(_transparentDrawData->vertexData);
		std::vector<DrawArraysIndirectCommand>().swap(_transparentDrawData->indirectBufferData);
		std::vector<uint32_t>().swap(_transparentDrawData->drawMeta);
	}
	return (int)_transpDrawCount;
	// removed dumps for transparent
	// Early-return above; code below is never executed in template path
	// Keeping function structure minimal for now while stabilizing visuals.
}

int ChunkRenderer::renderTransparentBlocksNoCullForShadow()
{
	if (!_transparentDrawData) return 0;
	// Do not update/upload; assume previous frame uploaded buffers exist
	if (_transpDrawCount == 0) return 0;

	// Bind per-pass SSBOs: use SOURCE positions (binding=3) so no compute is required
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, _transpPosSrcSSBO);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, _transpInstSSBO);
	// Use SOURCE meta here to match the template commands (no compaction)
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 7, _transpMetaSrcSSBO);

	glDisable(GL_CULL_FACE);
	glBindVertexArray(_transparentVao);
	glBindBuffer(GL_DRAW_INDIRECT_BUFFER, _transpTemplIndirectBuffer);
	glMultiDrawArraysIndirect(GL_TRIANGLE_STRIP, nullptr, _transpDrawCount,
							 sizeof(DrawArraysIndirectCommand));
	glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
	glBindVertexArray(0);
	return (int)_transpDrawCount;
}

// GPU side frustum culling helpers (init and run)
void ChunkRenderer::initGpuCulling() {
	_cullProgram  = compileComputeShader("shaders/compute/frustum_cull.glsl");
	_locNumDraws  = glGetUniformLocation(_cullProgram, "numDraws");
	_locChunkSize = glGetUniformLocation(_cullProgram, "chunkSize");
	_locUseOcclu  = glGetUniformLocation(_cullProgram, "useOcclusion");
	_locDepthTex  = glGetUniformLocation(_cullProgram, "depthTex");
	_locViewport  = glGetUniformLocation(_cullProgram, "viewport");
	_locView      = glGetUniformLocation(_cullProgram, "view");
	_locProj      = glGetUniformLocation(_cullProgram, "proj");
	_locCamPos    = glGetUniformLocation(_cullProgram, "camPos");
	_locDebugLogOcclu = glGetUniformLocation(_cullProgram, "debugLogOcclusion");
	_locHystThreshold = glGetUniformLocation(_cullProgram, "hystThreshold");
	_locRevealThreshold = glGetUniformLocation(_cullProgram, "revealThreshold");

	// Frustum UBO (6 vec4)
	glCreateBuffers(1, &_frustumUBO);
	glNamedBufferData(_frustumUBO, sizeof(glm::vec4) * 6, nullptr, GL_DYNAMIC_DRAW);
}

void ChunkRenderer::runGpuCulling(bool transparent) {
	GLuint templ = transparent ? _transpTemplIndirectBuffer : _templIndirectBuffer;
	GLuint out   = transparent ? _transparentIndirectBuffer : _indirectBuffer;
	GLsizei count = transparent ? _transpDrawCount : _solidDrawCount;
	if (count <= 0) return;

	// reset counter to 0 using orphaning to avoid racing on in-use storage
	GLuint zero = 0;
	GLuint param = transparent ? _transpParamsBuf : _solidParamsBuf;
	glNamedBufferData(param, sizeof(GLuint), &zero, GL_DYNAMIC_DRAW);

	glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);

	GLint prev = 0; glGetIntegerv(GL_CURRENT_PROGRAM, &prev);
	glUseProgram(_cullProgram);
	GLuint posSrc = transparent ? _transpPosSrcSSBO : _solidPosSrcSSBO; // binding 0
	GLuint posDst = transparent ? _transpPosSSBO    : _solidPosSSBO;    // binding 6
	GLuint metaSrc= transparent ? _transpMetaSrcSSBO: _solidMetaSrcSSBO; // binding 7
	GLuint metaDst= transparent ? _transpMetaSSBO   : _solidMetaSSBO;    // binding 8

	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, posSrc); // read
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, templ);  // read
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, out);    // write (commands)
	glBindBufferBase(GL_UNIFORM_BUFFER,        3, _frustumUBO);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, param);  // counter
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 6, posDst); // write (compacted posRes)
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 7, metaSrc); // read meta
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 8, metaDst); // write compacted meta

	// Ensure hysteresis buffer exists and is sized/reset as needed
	if (_hystSSBO == 0) glCreateBuffers(1, &_hystSSBO);
	GLsizeiptr hystBytesNeeded = (GLsizeiptr)count * (GLsizeiptr)sizeof(GLuint);
	if (_capHyst < hystBytesNeeded) {
		GLsizeiptr cap = _capHyst > 0 ? _capHyst : (GLsizeiptr)256;
		while (cap < hystBytesNeeded) cap *= 2;
		glNamedBufferData(_hystSSBO, cap, nullptr, GL_DYNAMIC_DRAW);
		_capHyst = cap;
		_hystDrawsStored = 0; // force zeroing below
	}
	if (_hystDrawsStored != (GLsizei)count) {
		// Reset counters to zero when draw list size changes (mapping may change)
		std::vector<GLuint> zeros(count, 0u);
		glNamedBufferSubData(_hystSSBO, 0, hystBytesNeeded, zeros.data());
		_hystDrawsStored = (GLsizei)count;
	}
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 11, _hystSSBO); // read/write

	// Ensure reveal-hold buffer exists and is sized/reset as needed
	if (_revealSSBO == 0) glCreateBuffers(1, &_revealSSBO);
	GLsizeiptr revealBytesNeeded = (GLsizeiptr)count * (GLsizeiptr)sizeof(GLuint);
	if (_capReveal < revealBytesNeeded) {
		GLsizeiptr cap = _capReveal > 0 ? _capReveal : (GLsizeiptr)256;
		while (cap < revealBytesNeeded) cap *= 2;
		glNamedBufferData(_revealSSBO, cap, nullptr, GL_DYNAMIC_DRAW);
		_capReveal = cap;
		_revealDrawsStored = 0;
	}
	if (_revealDrawsStored != (GLsizei)count) {
		std::vector<GLuint> zeros(count, 0u);
		glNamedBufferSubData(_revealSSBO, 0, revealBytesNeeded, zeros.data());
		_revealDrawsStored = (GLsizei)count;
	}
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 12, _revealSSBO);

	glUniform1ui(_locNumDraws, (GLuint)count);
	glUniform1f (_locChunkSize, (float)CHUNK_SIZE);
	// Enable occlusion only when a valid previous-frame depth & transforms are available
	if (_locUseOcclu >= 0) glUniform1i(_locUseOcclu, _occAvailable ? 1 : 0);
	if (_locHystThreshold >= 0) glUniform1ui(_locHystThreshold, 2u);
	if (_locRevealThreshold >= 0) glUniform1ui(_locRevealThreshold, 2u);

	// Disable Hi-Z debug recording to avoid GPU-CPU sync stalls
	if (_locDebugLogOcclu >= 0) glUniform1i(_locDebugLogOcclu, 0);
	if (_occAvailable) {
		if (_locDepthTex >= 0) {
			// Bind to unit 7
			glBindTextureUnit(7, _occDepthTex);
			glUniform1i(_locDepthTex, 7);
		}
		if (_locViewport >= 0) glUniform2f(_locViewport, (float)_occW, (float)_occH);
		if (_locView >= 0)     glUniformMatrix4fv(_locView, 1, GL_FALSE, glm::value_ptr(_occView));
		if (_locProj >= 0)     glUniformMatrix4fv(_locProj, 1, GL_FALSE, glm::value_ptr(_occProj));
		if (_locCamPos >= 0)   glUniform3fv(_locCamPos, 1, glm::value_ptr(_occCamPos));
	}

	GLuint groups = (count + 63u) / 64u;
	glDispatchCompute(groups, 1, 1);

	glMemoryBarrier(GL_COMMAND_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT);
	glUseProgram(prev ? (GLuint)prev : 0);

	// Heavy debug aggregation removed; this avoids large readbacks each frame.
}

// OpenGL setup for culling and rendering
void ChunkRenderer::initGLBuffer()
{
	if (_hasBufferInitialized) return;

	glGenVertexArrays(1, &_vao);
	glGenBuffers(1, &_vbo);
	glGenBuffers(1, &_indirectBuffer);

	// Static quad strip
	GLint vertices[] = { 0,0,0, 1,0,0, 0,1,0, 1,1,0 };

	// Solid VAO
	glBindVertexArray(_vao);
	glBindBuffer(GL_ARRAY_BUFFER, _vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
	glVertexAttribIPointer(0, 3, GL_INT, 3 * sizeof(GLint), (void*)0);
	glEnableVertexAttribArray(0);
	glBindVertexArray(0);

	// Transparent VAO
	glGenVertexArrays(1, &_transparentVao);
	glGenBuffers(1, &_transparentIndirectBuffer);

	glBindVertexArray(_transparentVao);
	glBindBuffer(GL_ARRAY_BUFFER, _vbo);
	glVertexAttribIPointer(0, 3, GL_INT, 3 * sizeof(GLint), (void*)0);
	glEnableVertexAttribArray(0);
	glBindVertexArray(0);

	// Create template SSBOs for compute inputs
	glCreateBuffers(1, &_templIndirectBuffer);
	glCreateBuffers(1, &_transpTemplIndirectBuffer);

	// Parameter buffers store the draw count written by the compute culling pass.
	// Use glNamedBufferData to orphan storage safely when resetting.
	glCreateBuffers(1, &_solidParamsBuf);
	glNamedBufferData(_solidParamsBuf, sizeof(GLuint), nullptr, GL_DYNAMIC_DRAW);
	glCreateBuffers(1, &_transpParamsBuf);
	glNamedBufferData(_transpParamsBuf, sizeof(GLuint), nullptr, GL_DYNAMIC_DRAW);
	initGpuCulling();
	_hasBufferInitialized = true;
}

// Helper to send vertices to GPU before rendering stage
// For both solid and transparent
void ChunkRenderer::pushVerticesToOpenGL(bool transparent)
{
	std::lock_guard<std::mutex> lock(_solidDrawDataMutex);
	// Ensure all previous GPU work that could use these buffers has completed
	if (_uploadGuard) {
		glClientWaitSync(_uploadGuard, GL_SYNC_FLUSH_COMMANDS_BIT, GLuint64(1000000000));
		glDeleteSync(_uploadGuard);
		_uploadGuard = 0;
	}
	auto ensureCapacityOnly = [](GLuint buf, GLsizeiptr& cap, GLsizeiptr neededBytes, GLenum usage)
	{
		// Grow when needed, and also opportunistically shrink when the buffer is
		// much larger than current content to reduce RAM usage from stale peaks.
		if (cap < neededBytes) {
			GLsizeiptr newCap = cap > 0 ? cap : (GLsizeiptr)4096;
			while (newCap < neededBytes) newCap *= 2;
			glNamedBufferData(buf, newCap, nullptr, usage);
			cap = newCap;
		} else if (cap > 0 && neededBytes >= 0) {
			// Shrink if grown far beyond current needs.
			// Threshold: if needed < 1/4 capacity and capacity > 1MB
			const GLsizeiptr oneMB = (GLsizeiptr)1 << 20;
			if (cap > oneMB && neededBytes < (cap / 4)) {
				GLsizeiptr newCap = (GLsizeiptr)4096;
				while (newCap < neededBytes) newCap *= 2;
				glNamedBufferData(buf, newCap, nullptr, usage);
				cap = newCap;
			}
		}
	};

	// Helper to orphan-and-upload each update to avoid races with driver threads
	auto ensureCapacityAndUpload = [](GLuint buf, GLsizeiptr& cap, GLsizeiptr neededBytes, const void* data, GLenum usage)
	{
		if (neededBytes <= 0) {
			// keep minimal non-zero storage
			glNamedBufferData(buf, 1, nullptr, usage);
			cap = 1;
			return;
		}
		// Orphan storage and upload new content in one step
		glNamedBufferData(buf, neededBytes, data, usage);
		cap = neededBytes;
	};

	if (transparent)
	{
	const GLsizeiptr nCmd      = (GLsizeiptr)_transparentDrawData->indirectBufferData.size();
	const GLsizeiptr bytesCmd  = nCmd * sizeof(DrawArraysIndirectCommand);
	const GLsizeiptr nInst     = (GLsizeiptr)_transparentDrawData->vertexData.size();
	const GLsizeiptr bytesInst = nInst * sizeof(int);
	const GLsizeiptr bytesSSBO = (GLsizeiptr)_transparentDrawData->ssboData.size() * sizeof(glm::vec4);
	const GLsizeiptr bytesMeta = (GLsizeiptr)_transparentDrawData->drawMeta.size() * sizeof(uint32_t);

		// Ensure buffers exist
		if (!_transpInstSSBO) glCreateBuffers(1, &_transpInstSSBO);
		if (!_transpPosSSBO)  glCreateBuffers(1, &_transpPosSSBO);     // DESTINATION (compute writes)
	if (!_transpPosSrcSSBO) glCreateBuffers(1, &_transpPosSrcSSBO); // SOURCE (uploaded here)

		// Upload template + out commands (as before)
		ensureCapacityAndUpload(_transpTemplIndirectBuffer, _capTemplTranspCmd, bytesCmd,
			_transparentDrawData->indirectBufferData.data(), GL_STATIC_DRAW);
		ensureCapacityAndUpload(_transparentIndirectBuffer, _capOutTranspCmd, bytesCmd,
			_transparentDrawData->indirectBufferData.data(), GL_DYNAMIC_DRAW);

		// Upload instance SSBO (binding=4)
	ensureCapacityAndUpload(_transpInstSSBO, _capTranspInst, bytesInst,
		_transparentDrawData->vertexData.data(), GL_DYNAMIC_DRAW);

		// Upload SOURCE pos SSBO (binding=0 for compute)
	ensureCapacityAndUpload(_transpPosSrcSSBO, _capTranspSSBOSrc, bytesSSBO,
		_transparentDrawData->ssboData.data(), GL_DYNAMIC_DRAW);

	// Upload SOURCE meta (compute reads), ensure DEST meta (compute writes)
	if (!_transpMetaSrcSSBO) glCreateBuffers(1, &_transpMetaSrcSSBO);
	ensureCapacityAndUpload(_transpMetaSrcSSBO, _capTranspMetaSrc, bytesMeta,
		_transparentDrawData->drawMeta.data(), GL_DYNAMIC_DRAW);
	if (!_transpMetaSSBO) glCreateBuffers(1, &_transpMetaSSBO);
	ensureCapacityOnly(_transpMetaSSBO, _capTranspMeta, bytesMeta, GL_DYNAMIC_DRAW);

		// Ensure DESTINATION pos SSBO (binding=6 for compute, later bound for draw)
		// No SubData upload; compute shader fills it.
		ensureCapacityOnly(_transpPosSSBO, _capTranspSSBO, bytesSSBO, GL_DYNAMIC_DRAW);

		_transpDrawCount = (GLsizei)nCmd;
		_needTransparentUpdate = false;
	}
	else
	{
	const GLsizeiptr nCmd      = (GLsizeiptr)_solidDrawData->indirectBufferData.size();
	const GLsizeiptr bytesCmd  = nCmd * sizeof(DrawArraysIndirectCommand);
	const GLsizeiptr nInst     = (GLsizeiptr)_solidDrawData->vertexData.size();
	const GLsizeiptr bytesInst = nInst * sizeof(int);
	const GLsizeiptr bytesSSBO = (GLsizeiptr)_solidDrawData->ssboData.size() * sizeof(glm::vec4);
	const GLsizeiptr bytesMeta = (GLsizeiptr)_solidDrawData->drawMeta.size() * sizeof(uint32_t);

		// Ensure buffers exist
		if (!_solidInstSSBO)  glCreateBuffers(1, &_solidInstSSBO);
		if (!_solidPosSSBO)   glCreateBuffers(1, &_solidPosSSBO);      // DESTINATION
		if (!_solidPosSrcSSBO) glCreateBuffers(1, &_solidPosSrcSSBO);  // SOURCE

		// Upload template and out command buffers
		ensureCapacityAndUpload(_templIndirectBuffer, _capTemplSolidCmd, bytesCmd,
			_solidDrawData->indirectBufferData.data(), GL_STATIC_DRAW);
		ensureCapacityAndUpload(_indirectBuffer, _capOutSolidCmd, bytesCmd,
			_solidDrawData->indirectBufferData.data(), GL_DYNAMIC_DRAW);

		// Upload instance SSBO (binding=4)
	ensureCapacityAndUpload(_solidInstSSBO, _capSolidInst, bytesInst,
		_solidDrawData->vertexData.data(), GL_DYNAMIC_DRAW);

		// Upload SOURCE pos SSBO (binding=0 for compute)
	ensureCapacityAndUpload(_solidPosSrcSSBO, _capSolidSSBOSrc, bytesSSBO,
		_solidDrawData->ssboData.data(), GL_DYNAMIC_DRAW);

	// Upload SOURCE meta (compute reads), ensure DEST meta
	if (!_solidMetaSrcSSBO) glCreateBuffers(1, &_solidMetaSrcSSBO);
	ensureCapacityAndUpload(_solidMetaSrcSSBO, _capSolidMetaSrc, bytesMeta,
		_solidDrawData->drawMeta.data(), GL_DYNAMIC_DRAW);
	if (!_solidMetaSSBO) glCreateBuffers(1, &_solidMetaSSBO);
	ensureCapacityOnly(_solidMetaSSBO, _capSolidMeta, bytesMeta, GL_DYNAMIC_DRAW);

		// Ensure DESTINATION pos SSBO (binding=6 for compute, later bound for draw)
		ensureCapacityOnly(_solidPosSSBO, _capSolidSSBO, bytesSSBO, GL_DYNAMIC_DRAW);

		_solidDrawCount = (GLsizei)nCmd;
		// Compute triangles count once while CPU data is present (unchanged)
		long long tris = 0;
		for (const auto& c : _solidDrawData->indirectBufferData) {
			long long per = (c.count >= 3) ? (c.count - 2) : 0;
			tris += (long long)c.instanceCount * per;
		}
		_lastSolidTris = tris;
		_needUpdate = false;
	}
}
