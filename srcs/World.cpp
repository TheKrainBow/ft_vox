#include "World.hpp"
#include "define.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

static inline int mod_floor(int a, int b) {
	int r = a % b;
	return (r < 0) ? r + b : r;
}
static inline int floor_div(int a, int b) {
	int q = a / b, r = a % b;
	return (r && ((r < 0) != (b < 0))) ? (q - 1) : q;
}

World::World(int seed, Camera &camera, ThreadPool &pool, std::atomic_bool *isRunning)
: _threadPool(pool),
_camera(camera),
_isRunning(isRunning)
{
	_needUpdate = true;
	_needTransparentUpdate = true;
	_hasBufferInitialized = false;
	_drawData = nullptr;
	_transparentDrawData = nullptr;
}

World::~World()
{
	delete _drawData;
	delete _transparentDrawData;
}

void World::shutdownGL()
{
	// Delete GL programs and buffers created by World
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
}

bool World::getIsRunning() {
	// During early construction, running mutex/flag may not be wired yet
	if (_isRunning == nullptr)
		return true;
	return *_isRunning;
}

void World::updateDrawData()
{
	std::lock_guard<std::mutex> lock(_drawDataMutex);

	// queues: keep only the newest staged data to minimize uploads
	while (getIsRunning() && _stagedDataQueue.size() > 1)
	{
		DisplayData *old = _stagedDataQueue.front();
		_stagedDataQueue.pop();
		delete old;
	}
	while (getIsRunning() && _transparentStagedDataQueue.size() > 1)
	{
		DisplayData *old = _transparentStagedDataQueue.front();
		_transparentStagedDataQueue.pop();
		delete old;
	}
	if (!_stagedDataQueue.empty())
	{
		DisplayData *stagedData = _stagedDataQueue.front();
		std::swap(stagedData, _drawData);
		delete stagedData;
		stagedData = nullptr;
		_stagedDataQueue.pop();
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
	if (_drawData) {
		if (!_drawData->indirectBufferData.empty() &&
			_drawData->ssboData.size() != _drawData->indirectBufferData.size()) {
			std::cout << "[CULL] Mismatch solid: ssbo=" << _drawData->ssboData.size()
						<< " cmds=" << _drawData->indirectBufferData.size() << std::endl;
		}
	}
	// Transparent sanity (uses the same SSBO)
	if (_transparentDrawData) {
		if (_drawData && !_transparentDrawData->indirectBufferData.empty() &&
			_drawData->ssboData.size() != _transparentDrawData->indirectBufferData.size()) {
			std::cout << "[CULL] Mismatch transp: ssbo=" << _drawData->ssboData.size()
						<< " cmds=" << _transparentDrawData->indirectBufferData.size() << std::endl;
		}
	}
}

int World::displaySolid()
{
	if (!getIsRunning()) return 0;
	if (!_drawData) return 0;
	if (_needUpdate) { pushVerticesToOpenGL(false); }
	if (_solidDrawCount == 0) return 0;

	runGpuCulling(false);

	// Bind per-pass SSBOs (single-buffer path)
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, _solidPosSSBO);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, _solidInstSSBO);

	glBindVertexArray(_vao);
	glBindBuffer(GL_DRAW_INDIRECT_BUFFER, _indirectBuffer);
	glBindBuffer(GL_PARAMETER_BUFFER_ARB, _solidParamsBuf); // contains uint drawCount
	glMultiDrawArraysIndirectCount(GL_TRIANGLE_STRIP, /*indirect*/ nullptr,
								   /*drawcount*/ 0, /*maxcount*/ _solidDrawCount,
								   sizeof(DrawArraysIndirectCommand));
	glBindBuffer(GL_PARAMETER_BUFFER_ARB, 0);
	glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);

	// Prefer cached triangles count if CPU buffers were discarded
	long long tris = 0;
	if (!_drawData->indirectBufferData.empty()) {
		for (const auto& c : _drawData->indirectBufferData) {
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
		_drawData->vertexData.clear();
		_drawData->indirectBufferData.clear();
		std::vector<int>().swap(_drawData->vertexData);
		std::vector<DrawArraysIndirectCommand>().swap(_drawData->indirectBufferData);
	}
	return (int)tris;
}

int World::displayTransparent()
{
	if (!getIsRunning()) return 0;
	if (!_transparentDrawData) return 0;
	if (_needTransparentUpdate) { pushVerticesToOpenGL(true); }
	if (_transpDrawCount == 0) return 0;

	runGpuCulling(true);

	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, _transpPosSSBO);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, _transpInstSSBO);

	glDisable(GL_CULL_FACE);
	glBindVertexArray(_transparentVao);
	glBindBuffer(GL_DRAW_INDIRECT_BUFFER, _transparentIndirectBuffer);
	glBindBuffer(GL_PARAMETER_BUFFER_ARB, _transpParamsBuf);
	glMultiDrawArraysIndirectCount(GL_TRIANGLE_STRIP, nullptr, 0, _transpDrawCount,
								   sizeof(DrawArraysIndirectCommand));
	glBindBuffer(GL_PARAMETER_BUFFER_ARB, 0);
	glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);

	// After transparent upload/draw, CPU-side transparent draw data is no longer needed
	if (!_needTransparentUpdate) {
		_transparentDrawData->vertexData.clear();
		_transparentDrawData->indirectBufferData.clear();
		std::vector<int>().swap(_transparentDrawData->vertexData);
		std::vector<DrawArraysIndirectCommand>().swap(_transparentDrawData->indirectBufferData);
		// We can also drop ssboData now as both passes have uploaded positions
		if (_drawData) {
			_drawData->ssboData.clear();
			std::vector<vec4>().swap(_drawData->ssboData);
		}
	}
	return (int)_transpDrawCount;
}

void World::pushVerticesToOpenGL(bool transparent)
{
	std::lock_guard<std::mutex> lock(_drawDataMutex);
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
		const GLsizeiptr bytesSSBO = (GLsizeiptr)_drawData->ssboData.size() * sizeof(glm::vec4);

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
			_drawData->ssboData.data(), GL_DYNAMIC_DRAW);

		// Ensure DESTINATION pos SSBO (binding=6 for compute, later bound for draw)
		// NOTE: no SubData upload; compute shader fills it.
		ensureCapacityOnly(_transpPosSSBO, _capTranspSSBO, bytesSSBO, GL_DYNAMIC_DRAW);

		_transpDrawCount = (GLsizei)nCmd;
		_needTransparentUpdate = false;
	}
	else
	{
		const GLsizeiptr nCmd      = (GLsizeiptr)_drawData->indirectBufferData.size();
		const GLsizeiptr bytesCmd  = nCmd * sizeof(DrawArraysIndirectCommand);
		const GLsizeiptr nInst     = (GLsizeiptr)_drawData->vertexData.size();
		const GLsizeiptr bytesInst = nInst * sizeof(int);
		const GLsizeiptr bytesSSBO = (GLsizeiptr)_drawData->ssboData.size() * sizeof(glm::vec4);

		// Ensure buffers exist
		if (!_solidInstSSBO)  glCreateBuffers(1, &_solidInstSSBO);
		if (!_solidPosSSBO)   glCreateBuffers(1, &_solidPosSSBO);      // DESTINATION
		if (!_solidPosSrcSSBO) glCreateBuffers(1, &_solidPosSrcSSBO);  // SOURCE

		// Upload template and out command buffers
		ensureCapacityAndUpload(_templIndirectBuffer, _capTemplSolidCmd, bytesCmd,
			_drawData->indirectBufferData.data(), GL_STATIC_DRAW);
		ensureCapacityAndUpload(_indirectBuffer, _capOutSolidCmd, bytesCmd,
			_drawData->indirectBufferData.data(), GL_DYNAMIC_DRAW);

		// Upload instance SSBO (binding=4)
		ensureCapacityAndUpload(_solidInstSSBO, _capSolidInst, bytesInst,
			_drawData->vertexData.data(), GL_DYNAMIC_DRAW);

		// Upload SOURCE pos SSBO (binding=0 for compute)
		ensureCapacityAndUpload(_solidPosSrcSSBO, _capSolidSSBOSrc, bytesSSBO,
			_drawData->ssboData.data(), GL_DYNAMIC_DRAW);

		// Ensure DESTINATION pos SSBO (binding=6 for compute, later bound for draw)
		ensureCapacityOnly(_solidPosSSBO, _capSolidSSBO, bytesSSBO, GL_DYNAMIC_DRAW);

		_solidDrawCount = (GLsizei)nCmd;
		// Compute triangles count once while CPU data is present (unchanged)
		long long tris = 0;
		for (const auto& c : _drawData->indirectBufferData) {
			long long per = (c.count >= 3) ? (c.count - 2) : 0;
			tris += (long long)c.instanceCount * per;
		}
		_lastSolidTris = tris;
		_needUpdate = false;
	}
}

void World::initGpuCulling() {
	_cullProgram  = compileComputeShader("shaders/compute/frustum_cull.glsl");
	_locNumDraws  = glGetUniformLocation(_cullProgram, "numDraws");
	_locChunkSize = glGetUniformLocation(_cullProgram, "chunkSize");

	// Frustum UBO (6 vec4)
	glCreateBuffers(1, &_frustumUBO);
	glNamedBufferData(_frustumUBO, sizeof(glm::vec4) * 6, nullptr, GL_DYNAMIC_DRAW);
}

void World::initGLBuffer()
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

	glCreateBuffers(1, &_solidParamsBuf);
	glNamedBufferStorage(_solidParamsBuf, sizeof(GLuint), nullptr,
		GL_DYNAMIC_STORAGE_BIT | GL_MAP_WRITE_BIT);
	glCreateBuffers(1, &_transpParamsBuf);
	glNamedBufferStorage(_transpParamsBuf, sizeof(GLuint), nullptr,
		GL_DYNAMIC_STORAGE_BIT | GL_MAP_WRITE_BIT);
	initGpuCulling();
	_hasBufferInitialized = true;
}

void World::setViewProj(const glm::mat4& view, const glm::mat4& proj) {
	Frustum f = Frustum::fromVP(proj * view);
	glm::vec4 planes[6];
	for (int i = 0; i < 6; ++i)
		planes[i] = glm::vec4(f.p[i].n, f.p[i].d);
	glNamedBufferSubData(_frustumUBO, 0, sizeof(planes), planes);
	{
		std::lock_guard<std::mutex> lk(_frustumMutex);
		_cachedFrustum = f;
		_hasCachedFrustum = true;
	}
}

void World::runGpuCulling(bool transparent) {
	if (!getIsRunning())
		return ;
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

	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, posSrc); // read
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, templ);  // read
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, out);    // write (commands)
	glBindBufferBase(GL_UNIFORM_BUFFER,        3, _frustumUBO);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, param);  // counter
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 6, posDst); // write (compacted posRes)

	glUniform1ui(_locNumDraws, (GLuint)count);
	glUniform1f (_locChunkSize, (float)CHUNK_SIZE);

	GLuint groups = (count + 63u) / 64u;
	glDispatchCompute(groups, 1, 1);

	glMemoryBarrier(GL_COMMAND_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT);
	glUseProgram(prev ? (GLuint)prev : 0);
}

static inline bool isSolidDeletable(BlockType b) {
	return (b != AIR && b != WATER && b != BEDROCK);
}

bool World::raycastHit(const glm::vec3& originWorld,
						const glm::vec3& dirWorld,
						float maxDistance,
						glm::ivec3& outBlock)
{
	if (maxDistance <= 0.0f) return false;

	glm::vec3 dir = dirWorld;
	float dlen = glm::length(dir);
	if (dlen < 1e-8f) return false;
	dir /= dlen;

	glm::vec3 ro = originWorld + dir * 0.001f; // nudge
	glm::ivec3 voxel(
		(int)std::floor(ro.x),
		(int)std::floor(ro.y),
		(int)std::floor(ro.z)
	);

	glm::ivec3 step(
		(dir.x > 0.f) ? 1 : (dir.x < 0.f) ? -1 : 0,
		(dir.y > 0.f) ? 1 : (dir.y < 0.f) ? -1 : 0,
		(dir.z > 0.f) ? 1 : (dir.z < 0.f) ? -1 : 0
	);

	const float INF = std::numeric_limits<float>::infinity();
	glm::vec3 tDelta(
		(step.x != 0) ? 1.0f / std::abs(dir.x) : INF,
		(step.y != 0) ? 1.0f / std::abs(dir.y) : INF,
		(step.z != 0) ? 1.0f / std::abs(dir.z) : INF
	);

	auto firstBoundaryT = [&](float r, int v, int s, float d) -> float {
		if (s > 0) return ((float(v) + 1.0f) - r) / d;
		if (s < 0) return (r - float(v)) / -d;
		return INF;
	};
	glm::vec3 tMaxV(
		firstBoundaryT(ro.x, voxel.x, step.x, dir.x),
		firstBoundaryT(ro.y, voxel.y, step.y, dir.y),
		firstBoundaryT(ro.z, voxel.z, step.z, dir.z)
	);

	float t = 0.0f;
	const int MAX_STEPS = 512;

	for (int i = 0; i < MAX_STEPS; ++i) {
		// step to next voxel
		if (tMaxV.x < tMaxV.y) {
			if (tMaxV.x < tMaxV.z)	{ voxel.x += step.x; t = tMaxV.x; tMaxV.x += tDelta.x; }
			else					{ voxel.z += step.z; t = tMaxV.z; tMaxV.z += tDelta.z; }
		} else {
			if (tMaxV.y < tMaxV.z)	{ voxel.y += step.y; t = tMaxV.y; tMaxV.y += tDelta.y; }
			else					{ voxel.z += step.z; t = tMaxV.z; tMaxV.z += tDelta.z; }
		}
		if (t > maxDistance) return false;

		ivec2 chunkPos(
			(int)std::floor((float)voxel.x / (float)CHUNK_SIZE),
			(int)std::floor((float)voxel.z / (float)CHUNK_SIZE)
		);
		BlockType b = getBlock(chunkPos, voxel);
		if (isSolidDeletable(b)) {
			outBlock = voxel;
			return true;
		}
	}
	return false;
}

BlockType World::raycastHitFetch(const glm::vec3& originWorld,
						const glm::vec3& dirWorld,
						float maxDistance,
						glm::ivec3& outBlock)
{
	if (maxDistance <= 0.0f) return AIR;

	glm::vec3 dir = dirWorld;
	float dlen = glm::length(dir);
	if (dlen < 1e-8f) return AIR;
	dir /= dlen;

	glm::vec3 ro = originWorld + dir * 0.001f; // nudge
	glm::ivec3 voxel(
		(int)std::floor(ro.x),
		(int)std::floor(ro.y),
		(int)std::floor(ro.z)
	);

	glm::ivec3 step(
		(dir.x > 0.f) ? 1 : (dir.x < 0.f) ? -1 : 0,
		(dir.y > 0.f) ? 1 : (dir.y < 0.f) ? -1 : 0,
		(dir.z > 0.f) ? 1 : (dir.z < 0.f) ? -1 : 0
	);

	const float INF = std::numeric_limits<float>::infinity();
	glm::vec3 tDelta(
		(step.x != 0) ? 1.0f / std::abs(dir.x) : INF,
		(step.y != 0) ? 1.0f / std::abs(dir.y) : INF,
		(step.z != 0) ? 1.0f / std::abs(dir.z) : INF
	);

	auto firstBoundaryT = [&](float r, int v, int s, float d) -> float {
		if (s > 0) return ((float(v) + 1.0f) - r) / d;
		if (s < 0) return (r - float(v)) / -d;
		return INF;
	};
	glm::vec3 tMaxV(
		firstBoundaryT(ro.x, voxel.x, step.x, dir.x),
		firstBoundaryT(ro.y, voxel.y, step.y, dir.y),
		firstBoundaryT(ro.z, voxel.z, step.z, dir.z)
	);

	float t = 0.0f;
	const int MAX_STEPS = 512;

	for (int i = 0; i < MAX_STEPS; ++i) {
		// step to next voxel
		if (tMaxV.x < tMaxV.y) {
			if (tMaxV.x < tMaxV.z)	{ voxel.x += step.x; t = tMaxV.x; tMaxV.x += tDelta.x; }
			else					{ voxel.z += step.z; t = tMaxV.z; tMaxV.z += tDelta.z; }
		} else {
			if (tMaxV.y < tMaxV.z)	{ voxel.y += step.y; t = tMaxV.y; tMaxV.y += tDelta.y; }
			else					{ voxel.z += step.z; t = tMaxV.z; tMaxV.z += tDelta.z; }
		}
		if (t > maxDistance) return AIR;

		ivec2 chunkPos(
			(int)std::floor((float)voxel.x / (float)CHUNK_SIZE),
			(int)std::floor((float)voxel.z / (float)CHUNK_SIZE)
		);
		BlockType b = getBlock(chunkPos, voxel);
		if (isSolidDeletable(b)) {
			outBlock = voxel;
			return b;
		}
	}
	return AIR;
}

bool World::raycastDeleteOne(const glm::vec3& originWorld,
				const glm::vec3& dirWorld,
				float maxDistance)
{
	glm::ivec3 hit;
	if (!raycastHit(originWorld, dirWorld, maxDistance, hit))
		return false;

	// Chunk that owns the hit voxel
	ivec2 chunkPos(
		(int)std::floor((float)hit.x / (float)CHUNK_SIZE),
		(int)std::floor((float)hit.z / (float)CHUNK_SIZE)
	);

	// Try to apply immediately (falls back to queue if chunk not ready)
	bool wroteNow = setBlockOrQueue(chunkPos, hit, AIR);

	if (wroteNow)
	{
		// 1) Rebuild current chunk mesh
		if (Chunk* c = getChunk(chunkPos))
		{
			c->setAsModified();
			c->sendFacesToDisplay();
		}

		// 2) If at border, rebuild neighbor meshes that share the broken face
		const int lx = (hit.x % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;
		const int lz = (hit.z % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;

		ivec2 neighbors[4];
		int n = 0;
		if (lx == 0)               neighbors[n++] = { chunkPos.x - 1, chunkPos.y };
		if (lx == CHUNK_SIZE - 1)  neighbors[n++] = { chunkPos.x + 1, chunkPos.y };
		if (lz == 0)               neighbors[n++] = { chunkPos.x,     chunkPos.y - 1 };
		if (lz == CHUNK_SIZE - 1)  neighbors[n++] = { chunkPos.x,     chunkPos.y + 1 };

		for (int i = 0; i < n; ++i)
			if (Chunk* nc = getChunk(neighbors[i]))
				nc->sendFacesToDisplay();

		// 3) Stage a fresh display snapshot off-thread (coalesced)
		scheduleDisplayUpdate();
	}
	return true;
}

bool World::raycastPlaceOne(const glm::vec3& originWorld,
				const glm::vec3& dirWorld,
				float maxDistance,
				BlockType block)
{
	// Perform a DDA raycast but place the block on the empty voxel in front
	// of the hit block (the voxel we were in before entering the solid block),
	// matching Minecraft behavior.

	if (maxDistance <= 0.0f) return false;

	// Normalize direction
	glm::vec3 dir = dirWorld;
	float dlen = glm::length(dir);
	if (dlen < 1e-8f) return false;
	dir /= dlen;

	// Nudge origin slightly to avoid self-intersection at boundaries
	glm::vec3 ro = originWorld + dir * 0.001f;

	// Current voxel and the previous voxel we were in before stepping
	glm::ivec3 voxel(
		(int)std::floor(ro.x),
		(int)std::floor(ro.y),
		(int)std::floor(ro.z)
	);
	glm::ivec3 prev = voxel;

	// Step for each axis
	glm::ivec3 step(
		(dir.x > 0.f) ? 1 : (dir.x < 0.f) ? -1 : 0,
		(dir.y > 0.f) ? 1 : (dir.y < 0.f) ? -1 : 0,
		(dir.z > 0.f) ? 1 : (dir.z < 0.f) ? -1 : 0
	);

	const float INF = std::numeric_limits<float>::infinity();
	glm::vec3 tDelta(
		(step.x != 0) ? 1.0f / std::abs(dir.x) : INF,
		(step.y != 0) ? 1.0f / std::abs(dir.y) : INF,
		(step.z != 0) ? 1.0f / std::abs(dir.z) : INF
	);

	auto firstBoundaryT = [&](float r, int v, int s, float d) -> float {
		if (s > 0) return ((float(v) + 1.0f) - r) / d;
		if (s < 0) return (r - float(v)) / -d;
		return INF;
	};

	glm::vec3 tMaxV(
		firstBoundaryT(ro.x, voxel.x, step.x, dir.x),
		firstBoundaryT(ro.y, voxel.y, step.y, dir.y),
		firstBoundaryT(ro.z, voxel.z, step.z, dir.z)
	);

	float t = 0.0f;
	const int MAX_STEPS = 512;

	for (int i = 0; i < MAX_STEPS; ++i) {
		// Step to next voxel, remember where we came from
		if (tMaxV.x < tMaxV.y) {
			if (tMaxV.x < tMaxV.z) { prev = voxel; voxel.x += step.x; t = tMaxV.x; tMaxV.x += tDelta.x; }
			else                   { prev = voxel; voxel.z += step.z; t = tMaxV.z; tMaxV.z += tDelta.z; }
		} else {
			if (tMaxV.y < tMaxV.z) { prev = voxel; voxel.y += step.y; t = tMaxV.y; tMaxV.y += tDelta.y; }
			else                   { prev = voxel; voxel.z += step.z; t = tMaxV.z; tMaxV.z += tDelta.z; }
		}

		if (t > maxDistance) return false;

		ivec2 chunkPos(
			(int)std::floor((float)voxel.x / (float)CHUNK_SIZE),
			(int)std::floor((float)voxel.z / (float)CHUNK_SIZE)
		);
		BlockType b = getBlock(chunkPos, voxel);
		if (b == BEDROCK || isSolidDeletable(b)) {
			// Place into the previous (empty) voxel we were in before entering the hit voxel
			ivec2 placeChunk(
				(int)std::floor((float)prev.x / (float)CHUNK_SIZE),
				(int)std::floor((float)prev.z / (float)CHUNK_SIZE)
			);

			// Prevent placing a block inside the player's occupied cells (feet/head)
			{
				glm::vec3 camW = _camera.getWorldPosition();
				int px = static_cast<int>(std::floor(camW.x));
				int pz = static_cast<int>(std::floor(camW.z));
				int footY = static_cast<int>(std::floor(camW.y - EYE_HEIGHT + EPS));
				glm::ivec3 feet = { px, footY, pz };
				glm::ivec3 head = { px, footY + 1, pz };
				if (prev == feet || prev == head) {
					return false; // would collide with player
				}
			}

			// Only place if target is empty or replaceable (AIR/WATER).
			BlockType current = getBlock(placeChunk, prev);
			if (!(current == AIR || current == WATER)) return false;

			bool wroteNow = setBlockOrQueue(placeChunk, prev, block);
			if (wroteNow) {
				// Rebuild current chunk mesh
				if (Chunk* c = getChunk(placeChunk))
				{
					c->setAsModified();
					c->sendFacesToDisplay();
				}

				// If at border, also rebuild neighbors that share faces
				const int lx = (prev.x % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;
				const int lz = (prev.z % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;

				ivec2 neighbors[4];
				int n = 0;
				if (lx == 0)               neighbors[n++] = { placeChunk.x - 1, placeChunk.y };
				if (lx == CHUNK_SIZE - 1)  neighbors[n++] = { placeChunk.x + 1, placeChunk.y };
				if (lz == 0)               neighbors[n++] = { placeChunk.x,     placeChunk.y - 1 };
				if (lz == CHUNK_SIZE - 1)  neighbors[n++] = { placeChunk.x,     placeChunk.y + 1 };

				for (int i2 = 0; i2 < n; ++i2)
					if (Chunk* nc = getChunk(neighbors[i2]))
						nc->sendFacesToDisplay();

				// Stage a fresh display snapshot off-thread (coalesced)
				scheduleDisplayUpdate();
			}
			return true;
		}
	}
	return false;
}

void World::scheduleDisplayUpdate() {
	if (_buildingDisplay) return;
	if (!getIsRunning()) return;
	_threadPool.enqueue(&World::updateFillData, this);
}

void World::getDisplayedChunksSnapshot(std::vector<ivec2>& out) {
	std::lock_guard<std::mutex> lk(_displayedChunksMutex);
	out.clear();
	out.reserve(_displayedChunks.size());
	for (const auto& kv : _displayedChunks) out.push_back(kv.first);
}
