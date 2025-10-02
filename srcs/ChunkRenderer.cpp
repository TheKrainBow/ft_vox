#include "ChunkRenderer.hpp"

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
}

ChunkRenderer::~ChunkRenderer()
{
	delete _solidDrawData;
	delete _transparentDrawData;
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
}

// Shared data setters
void ChunkRenderer::setViewProj(glm::vec4 planes[6]) {
	glNamedBufferSubData(_frustumUBO, 0, 6 * sizeof(glm::vec4), planes);
	// Reset occlusion source unless explicitly set for this view
	_occAvailable = false;
}

void ChunkRenderer::setOcclusionSource(GLuint depthTex, int width, int height,
									const glm::mat4& view, const glm::mat4& proj,
									const glm::vec3& camPos)
{
	// If draw data changed this frame, previous-frame depth may be invalid.
	// In that case, disable occlusion for this frame to avoid popping.
	bool disableOcclusionThisFrame = (_needUpdate.load() || _needTransparentUpdate.load());

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
	}
	if (!_transparentStagedDataQueue.empty())
	{
		
		DisplayData *stagedData = _transparentStagedDataQueue.front();
		std::swap(stagedData, _transparentDrawData);
		delete stagedData;
		stagedData = nullptr;
		_transparentStagedDataQueue.pop();
		_needTransparentUpdate = true;
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
		// No fresh commands this frame. If we have a last good count, draw those
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

	runGpuCulling(false);

    // Bind per-pass SSBOs (single-buffer path)
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, _solidPosSSBO);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, _solidInstSSBO);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 7, _solidMetaSSBO);

    glBindVertexArray(_vao);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, _indirectBuffer);
    glBindBuffer(GL_PARAMETER_BUFFER_ARB, _solidParamsBuf); // contains uint drawCount
    // removed dump diagnostics for solid
    // Defensive sanity: if the first few compacted commands are zero but we have a draw count,
    // fall back to using template commands for this frame to avoid flicker.
    if (_solidDrawCount > 0) {
        const size_t probe = std::min<size_t>(8, (size_t)_solidDrawCount);
        std::vector<DrawArraysIndirectCommand> head(probe);
        glGetNamedBufferSubData(_indirectBuffer, 0, probe * sizeof(DrawArraysIndirectCommand), head.data());
        bool allZero = true;
        for (size_t i = 0; i < probe; ++i) {
            if (head[i].instanceCount != 0u) { allZero = false; break; }
        }
        if (allZero) {
            GLsizeiptr bytesCmd = (GLsizeiptr)_solidDrawCount * sizeof(DrawArraysIndirectCommand);
            glCopyNamedBufferSubData(_templIndirectBuffer, _indirectBuffer, 0, 0, bytesCmd);
            // Also mirror per-draw meta so directions match the template commands
            GLsizeiptr bytesMeta = (GLsizeiptr)_solidDrawCount * sizeof(uint32_t);
            glCopyNamedBufferSubData(_solidMetaSrcSSBO, _solidMetaSSBO, 0, 0, bytesMeta);
            glMemoryBarrier(GL_COMMAND_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT);
            // silent fallback
        }
    }

    glMultiDrawArraysIndirectCount(GL_TRIANGLE_STRIP, /*indirect*/ nullptr,
                                   /*drawcount*/ 0, /*maxcount*/ _solidDrawCount,
                                   sizeof(DrawArraysIndirectCommand));

    // Safety fallback: if GPU culling produced zero draws OR we cannot read the
    // parameter buffer (no read mapping available), draw a conservative count
    // so we never end up with an empty frame.
	{
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
            // If mapping failed (driver/flags), prefer drawing rather than showing nothing.
            needFallback = true;
        }
        if (needFallback) {
            GLsizei count = _lastGoodSolidCount > 0 ? _lastGoodSolidCount : _solidDrawCount;
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
    // removed draw count log
    return (int)_transpDrawCount;
    // removed dumps for transparent
    // Note: we early-return above; code below is never executed in template path
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

	// Frustum UBO (6 vec4)
	glCreateBuffers(1, &_frustumUBO);
	glNamedBufferData(_frustumUBO, sizeof(glm::vec4) * 6, nullptr, GL_DYNAMIC_DRAW);
}

void ChunkRenderer::runGpuCulling(bool transparent) {
	GLuint templ = transparent ? _transpTemplIndirectBuffer : _templIndirectBuffer;
	GLuint out   = transparent ? _transparentIndirectBuffer : _indirectBuffer;
	GLsizei count = transparent ? _transpDrawCount : _solidDrawCount;
	if (count <= 0) return;

	// reset counter to 0
	GLuint zero = 0;
	GLuint param = transparent ? _transpParamsBuf : _solidParamsBuf;
	glNamedBufferSubData(param, 0, sizeof(GLuint), &zero);

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

	glUniform1ui(_locNumDraws, (GLuint)count);
	glUniform1f (_locChunkSize, (float)CHUNK_SIZE);
	if (_locUseOcclu >= 0) glUniform1i(_locUseOcclu, _occAvailable ? 1 : 0);
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
	// We also map them on the CPU to implement a safe fallback when the count is 0.
	// Ensure we request both READ and WRITE mapping rights so glMap* with READ works.
	glCreateBuffers(1, &_solidParamsBuf);
	glNamedBufferStorage(
		_solidParamsBuf,
		sizeof(GLuint),
		nullptr,
		GL_DYNAMIC_STORAGE_BIT | GL_MAP_WRITE_BIT | GL_MAP_READ_BIT
	);
	glCreateBuffers(1, &_transpParamsBuf);
	glNamedBufferStorage(
		_transpParamsBuf,
		sizeof(GLuint),
		nullptr,
		GL_DYNAMIC_STORAGE_BIT | GL_MAP_WRITE_BIT | GL_MAP_READ_BIT
	);
	initGpuCulling();
	_hasBufferInitialized = true;
}

// Helper to send vertices to GPU before rendering stage
// For both solid and transparent
void ChunkRenderer::pushVerticesToOpenGL(bool transparent)
{
	std::lock_guard<std::mutex> lock(_solidDrawDataMutex);
	auto ensureCapacityOnly = [](GLuint buf, GLsizeiptr& cap, GLsizeiptr neededBytes, GLenum usage)
	{
		if (cap < neededBytes)
		{
			GLsizeiptr newCap = cap > 0 ? cap : (GLsizeiptr)4096;
			while (newCap < neededBytes) newCap *= 2;
			glNamedBufferData(buf, newCap, nullptr, usage);
			cap = newCap;
		}
	};

	// Helper to grow buffers once and update with SubData (reduces realloc waiting)
	auto ensureCapacityAndUpload = [](GLuint buf, GLsizeiptr& cap, GLsizeiptr neededBytes, const void* data, GLenum usage)
	{
		if (cap < neededBytes)
		{
			GLsizeiptr newCap = cap > 0 ? cap : (GLsizeiptr)4096;
			while (newCap < neededBytes) newCap *= 2;
			glNamedBufferData(buf, newCap, nullptr, usage);
			cap = newCap;
		}
		if (neededBytes > 0)
			glNamedBufferSubData(buf, 0, neededBytes, data);
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
		if (!_transpPosSrcSSBO) glCreateBuffers(1, &_transpPosSrcSSBO); // SOURCE (we upload)

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
		// NOTE: no SubData upload; compute shader fills it.
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
