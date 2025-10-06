#include "StoneEngine.hpp"

#include <fstream>
#include <string>
#include <sstream>

#if defined(__linux__)
#include <unistd.h>
#endif

static glm::mat4 makeObliqueProjection(const glm::mat4 &proj,
									   const glm::mat4 &view,
									   const glm::vec4 &planeWorld)
{
	glm::vec4 planeVS = glm::transpose(glm::inverse(view)) * planeWorld;

	glm::mat4 P = proj;
	glm::vec4 q;
	q.x = (glm::sign(planeVS.x) + P[0][2]) / P[0][0];
	q.y = (glm::sign(planeVS.y) + P[1][2]) / P[1][1];
	q.z = -1.0f;
	q.w = (1.0f + P[2][2]) / P[2][3];

	glm::vec4 c = planeVS * (2.0f / glm::dot(planeVS, q));
	P[0][2] = c.x;
	P[1][2] = c.y;
	P[2][2] = c.z + 1.0f;
	P[3][2] = c.w;
	return P;
}

float mapRange(float x, float in_min, float in_max, float out_min, float out_max)
{
	return out_max - (x - in_min) * (out_max - out_min) / (in_max - in_min);
}

float mapExpo(float x, float in_min, float in_max, float out_min, float out_max)
{
	float t = (x - in_min) / (in_max - in_min);		 // normalize to [0, 1]
	return out_min * std::pow(out_max / out_min, t); // exponential interpolation
}

bool isTransparent(char block)
{
	// Treat CACTUS like LOG for face-visibility decisions so ground caps render
	// under the inset mesh (prevents a visible ring gap around the base).
	return block == AIR || block == WATER || block == LOG || block == CACTUS || block == LEAF || block == FLOWER_POPPY || block == FLOWER_DANDELION || block == FLOWER_CYAN || block == FLOWER_SHORT_GRASS || block == FLOWER_DEAD_BUSH;
}

// Display logs only if sides
bool faceDisplayCondition(char blockToDisplay, char neighborBlock, Direction dir)
{
	// For leaves: always show faces, but if neighbor is also a leaf, only
	// emit the face for positive-axis directions to avoid z-fighting between
	// coincident quads (keep one of the two faces).
	if (blockToDisplay == LEAF)
	{
		if (neighborBlock == LEAF)
		{
			return (dir == EAST || dir == UP || dir == SOUTH);
		}
		return true; // non-leaf neighbor: show the face regardless
	}

	// Apply the same neighboring-face rule used for logs to cactuses:
	// always render side faces even when adjacent to the same block type.
	const bool isLogOrCactus = (blockToDisplay == LOG || blockToDisplay == CACTUS);
	if (isLogOrCactus && dir <= EAST)
		return true;

	return (isTransparent(neighborBlock) && blockToDisplay != neighborBlock);
}

static inline void glResetActiveTextureTo0()
{
	glActiveTexture(GL_TEXTURE0);
}

static inline void glUnbindCommonTextures()
{
    // Unbind common targets
    glBindTexture(GL_TEXTURE_2D, 0);
	glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
	glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
}

static inline void glCleanupTextureState()
{
	glResetActiveTextureTo0();
	glUnbindCommonTextures();
}

void StoneEngine::updateFboWindowSize(PostProcessShader &shader)
{
	float texelX = 1.0f / windowWidth;
	float texelY = 1.0f / windowHeight;

	GLint texelSizeLoc = glGetUniformLocation(shader.program, "texelSize");
	glUseProgram(shader.program);
	glUniform2f(texelSizeLoc, texelX, texelY);
}

StoneEngine::StoneEngine(int seed, ThreadPool &pool) : camera(),
													   _pool(pool),
													   noise_gen(seed),
													   _chunkMgr(seed, &_isRunning, camera, chronoHelper, pool),
													   _player(camera, _chunkMgr)
{
	initData();
	initGLFW();
	initGLEW();
	initTextures();
	initRenderShaders();
	initShadowMapping();
	initDebugTextBox();
	initHelpTextBox();
	initFboShaders();
	reshapeAction(windowWidth, windowHeight);
	_chunkMgr.initGLBuffer();

	// Show a splash while the first mesh arrives
	_splashDeadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(LOADING_SPLASH_MS);
}

StoneEngine::~StoneEngine()
{
	// Ensure the GL context is current during teardown
	if (_window) glfwMakeContextCurrent(_window);
	// Drain any in-flight GPU work before deleting GL objects
	glFinish();
	// Ensure _chunkMgr GL resources are freed before destroying the context
	_chunkMgr.shutDownGL();

	// Teardown GL resources owned by helper classes while context is valid
	_skybox.shutdownGL();
	_textureManager.shutdownGL();
	debugBox.shutdownGL();
	helpBox.shutdownGL();
	_loadingBox.shutdownGL();

	glDeleteProgram(shaderProgram);
	glDeleteProgram(waterShaderProgram);
	glDeleteProgram(alphaShaderProgram);
	glDeleteProgram(sunShaderProgram);
	if (shadowShaderProgram) glDeleteProgram(shadowShaderProgram);
	glDeleteProgram(flowerProgram);

	// Clean up post-process shaders
	for (auto &[type, shader] : postProcessShaders)
	{
		glDeleteProgram(shader.program);
		glDeleteVertexArrays(1, &shader.vao);
		glDeleteBuffers(1, &shader.vbo);
	}
	postProcessShaders.clear();

	// Delete sun resources and water normal map
	if (sunVAO)
		glDeleteVertexArrays(1, &sunVAO);
	if (sunVBO)
		glDeleteBuffers(1, &sunVBO);
	if (waterNormalMap)
		glDeleteTextures(1, &waterNormalMap);

	// Delete wireframe/highlight resources
	if (_wireVAO)
		glDeleteVertexArrays(1, &_wireVAO);
	if (_wireVBO)
		glDeleteBuffers(1, &_wireVBO);
	if (_wireProgram)
		glDeleteProgram(_wireProgram);

	// Flowers resources
	if (flowerVAO)
		glDeleteVertexArrays(1, &flowerVAO);
	if (flowerVBO)
		glDeleteBuffers(1, &flowerVBO);
	if (flowerInstanceVBO)
		glDeleteBuffers(1, &flowerInstanceVBO);
	if (flowerTexture)
		glDeleteTextures(1, &flowerTexture);

	// Delete tmp and MSAA framebuffers/renderbuffers
	if (tmpFBO.fbo)
		glDeleteFramebuffers(1, &tmpFBO.fbo);
	if (tmpFBO.texture)
		glDeleteTextures(1, &tmpFBO.texture);
	if (tmpFBO.depth)
		glDeleteTextures(1, &tmpFBO.depth);

	// Delete shadow map resources
	if (shadowFBO) {
		glDeleteFramebuffers(1, &shadowFBO);
		shadowFBO = 0;
	}
	if (shadowMap) {
		glDeleteTextures(1, &shadowMap);
		shadowMap = 0;
	}

	if (msaaFBO.fbo)
		glDeleteFramebuffers(1, &msaaFBO.fbo);
	if (msaaFBO.texture)
		glDeleteRenderbuffers(1, &msaaFBO.texture);
	if (msaaFBO.depth)
		glDeleteRenderbuffers(1, &msaaFBO.depth);

	glDeleteTextures(1, &readFBO.texture);
	glDeleteTextures(1, &readFBO.depth);
	glDeleteFramebuffers(1, &readFBO.fbo);
	glDeleteTextures(1, &writeFBO.texture);
	glDeleteTextures(1, &writeFBO.depth);
	glDeleteFramebuffers(1, &writeFBO.fbo);

	// After deleting GL objects, flush and finish to let the driver
	// release any internal resources on its worker threads before
	// destroying the context/window (reduces TSan races in Mesa).
	glFlush();
	glFinish();
	// Terminate GLFW
	glfwDestroyWindow(_window);
	glfwTerminate();
}

void StoneEngine::run()
{
	_isRunning = true;

	// Set spawn point chunk and player pos
	_chunkMgr.initSpawn();

	// Run the orchestrator on a dedicated thread so it doesn't consume a pool worker
	std::thread chunkThread(&StoneEngine::updateChunkWorker, this);

	while (!glfwWindowShouldClose(_window))
	{
		glClear(GL_COLOR_BUFFER_BIT);
		update();
		glfwPollEvents();
	}
	{
		std::lock_guard<std::mutex> g(_isRunningMutex);
		_isRunning = false;
	}
	if (chunkThread.joinable())
		chunkThread.join();
}

void StoneEngine::initData()
{
	// Keys states and runtime booleans
	ignoreMouseEvent	= IGNORE_MOUSE;
	updateChunk			= ENABLE_WORLD_GENERATION;
	showTriangleMesh	= SHOW_TRIANGLES;
	mouseCaptureToggle	= CAPTURE_MOUSE;
	showDebugInfo		= SHOW_DEBUG;
	showHelp            = false;
	showUI				= SHOW_UI;
	showLight			= SHOW_LIGHTING;
	selectedBlockDebug	= air;
	gravity 			= GRAVITY;

	// Occlusion disabled window after edits
	_occlDisableFrames = 0;
	_prevOccValid = false;

	// Shadow throttle state
	_timeAccelerating = false;
	_shadowUpdateDivider = 20;
	_shadowUpdateCounter = 0;

	// Gets the max MSAA (anti aliasing) samples
	_maxSamples = 0;
	glGetIntegerv(GL_MAX_SAMPLES, &_maxSamples);

	if (SCHOOL_SAMPLES)
		_maxSamples = 8;

	// Window size
	windowHeight = W_HEIGHT;
	windowWidth = W_WIDTH;

	// FPS counter
	frameCount = 0;
	lastFrameTime = 0.0;
	currentFrameTime = 0.0;
	fps = 0.0;

	// Debug data
	drawnTriangles = 0.0;

	// Game data
	sunPosition = {0.0f, 0.0f, 0.0f};
	timeValue = 52000;
	camTopBlock.pos.x = 0.0;
	camTopBlock.pos.y = 0.0;
	camTopBlock.type = AIR;
	camTopBlock.height = 0;

	// Biome data
	_biome = 0;
	_humidity = 0.0;
	_temperature = 0.0;
}

void StoneEngine::initTextures()
{
	glEnable(GL_TEXTURE_2D);
	_textureManager.loadTexturesArray({
		{T_DIRT, "textures/dirt.ppm"},
		{T_COBBLE, "textures/cobble.ppm"},
		{T_STONE, "textures/stone.ppm"},
		{T_GRASS_SIDE, "textures/grass_block_side.ppm"},
		{T_GRASS_TOP, "textures/grass_block_top_colored.ppm"},
		{T_SAND, "textures/sand.ppm"},
		{T_WATER, "textures/water.ppm"},
		{T_SNOW, "textures/snow.ppm"},
		{T_BEDROCK, "textures/bedrock.ppm"},
		{T_LOG_SIDE, "textures/log_side.ppm"},
		{T_LOG_TOP, "textures/log_top.ppm"},
		{T_LEAF, "textures/leave.png"},
		{T_CACTUS_SIDE, "textures/cactus_side.ppm"},
		{T_CACTUS_TOP,  "textures/cactus_top.ppm"},
	});

	glGenTextures(1, &waterNormalMap);
	glBindTexture(GL_TEXTURE_2D, waterNormalMap);

	int width, height, nrChannels;
	unsigned char *data = stbi_load("textures/water_normal.jpg", &width, &height, &nrChannels, 0);

	if (data)
	{
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0,
					 GL_RGB, GL_UNSIGNED_BYTE, data);
		glGenerateMipmap(GL_TEXTURE_2D);
	}
	else
	{
		std::cerr << "Failed to load water normal map!" << std::endl;
	}
	stbi_image_free(data);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}

glm::vec3 StoneEngine::computeSunPosition(int timeValue, const glm::vec3 &cameraPos)
{
	const float pi = 3.14159265f;
	const float radius = 6000.0f;
	const float dayStart = 42000.0f;		  // sunrise
	const float dayLen = 86400.0f - dayStart; // 44400 (day duration)
	const float nightLen = dayStart;		  // 42000 (night duration)

	float t = static_cast<float>(timeValue);
	float angle;
	if (t < dayStart)
	{
		// Night: traverse pi..2pi across [0, dayStart)
		float phase = glm::clamp(t / nightLen, 0.0f, 1.0f);
		angle = pi + phase * pi;
	}
	else
	{
		// Day: traverse 0..pi across [dayStart, 86400]
		float phase = glm::clamp((t - dayStart) / dayLen, 0.0f, 1.0f);
		angle = phase * pi;
	}

	float x = radius * cos(angle);
	float y = radius * sin(angle); // y>0 during day, y<0 during night
	float z = 0.0f;

	// Return a position in the sun direction at a fixed radius from the camera,
	// used only for visual effects (sun sprite, god rays)
	return cameraPos + glm::vec3(x, y, z);
}

glm::vec3 StoneEngine::computeSunDirection(int timeValue)
{
	const float pi = 3.14159265f;
	const float dayStart = 42000.0f;          // sunrise
	const float dayLen   = 86400.0f - dayStart; // 44400 (day duration)
	const float nightLen = dayStart;           // 42000 (night duration)

	float t = static_cast<float>(timeValue);
	float angle;
	if (t < dayStart)
	{
		float phase = glm::clamp(t / nightLen, 0.0f, 1.0f);
		angle = pi + phase * pi;
	}
	else
	{
		float phase = glm::clamp((t - dayStart) / dayLen, 0.0f, 1.0f);
		angle = phase * pi;
	}

	// World-space directional light vector (pointing from world towards the sun)
	glm::vec3 dir = glm::normalize(glm::vec3(cos(angle), sin(angle), 0.0f));
	return dir;
}

void initSunQuad(GLuint &vao, GLuint &vbo)
{
	// Quad corners in NDC [-1, 1] (used as offset in clip space)
	float quadVertices[] = {
		-1.0f, -1.0f,
		-1.0f, 1.0f,
		1.0f, -1.0f,
		1.0f, 1.0f};

	glGenVertexArrays(1, &vao);
	glGenBuffers(1, &vbo);

	glBindVertexArray(vao);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);

	// Attribute 0 = vec2 aPos
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void *)0);
	glEnableVertexAttribArray(0);

	glBindVertexArray(0);
}

void StoneEngine::initWireframeResources()
{
	_wireProgram = createShaderProgram("shaders/postProcess/outline.vert", "shaders/postProcess/outline.frag");
	// 12 edges * 2 endpoints = 24 vertices (unit cube corners)
	const GLfloat lines[] = {
		// bottom rectangle (y=0)
		0,
		0,
		0,
		1,
		0,
		0,
		1,
		0,
		0,
		1,
		0,
		1,
		1,
		0,
		1,
		0,
		0,
		1,
		0,
		0,
		1,
		0,
		0,
		0,
		// top rectangle (y=1)
		0,
		1,
		0,
		1,
		1,
		0,
		1,
		1,
		0,
		1,
		1,
		1,
		1,
		1,
		1,
		0,
		1,
		1,
		0,
		1,
		1,
		0,
		1,
		0,
		// vertical pillars
		0,
		0,
		0,
		0,
		1,
		0,
		1,
		0,
		0,
		1,
		1,
		0,
		1,
		0,
		1,
		1,
		1,
		1,
		0,
		0,
		1,
		0,
		1,
		1,
	};

	glGenVertexArrays(1, &_wireVAO);
	glGenBuffers(1, &_wireVBO);
	glBindVertexArray(_wireVAO);
	glBindBuffer(GL_ARRAY_BUFFER, _wireVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(lines), lines, GL_STATIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);
	glEnableVertexAttribArray(0);
	glBindVertexArray(0);
}

void StoneEngine::initRenderShaders()
{
	shaderProgram = createShaderProgram("shaders/render/terrain.vert", "shaders/render/terrain.frag");
	waterShaderProgram = createShaderProgram("shaders/render/water.vert", "shaders/render/water.frag");
	alphaShaderProgram = createShaderProgram("shaders/render/alpha.vert", "shaders/render/alpha.frag");
	sunShaderProgram = createShaderProgram("shaders/render/sun.vert", "shaders/render/sun.frag");
	skyboxProgram = createShaderProgram("shaders/render/skybox.vert", "shaders/render/skybox.frag");
	shadowShaderProgram = createShaderProgram("shaders/render/terrain_shadow.vert", "shaders/render/terrain_shadow.frag");
	initSunQuad(sunVAO, sunVBO);
	initWireframeResources();
	initSkybox();
	initFlowerResources();
}

void StoneEngine::initShadowMapping()
{
	rebuildShadowResources(RENDER_DISTANCE);
}

void StoneEngine::rebuildShadowResources(int effectiveRender)
{
	(void)effectiveRender;

	// Clamp shadow map size to hardware limits to avoid incomplete FBOs
	GLint maxTex = 0;
	glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTex);
	if (maxTex > 0 && shadowMapSize > maxTex) {
		shadowMapSize = maxTex;
	}

	if (shadowFBO) {
		glDeleteFramebuffers(1, &shadowFBO);
		shadowFBO = 0;
	}
	if (shadowMap) {
		glDeleteTextures(1, &shadowMap);
		shadowMap = 0;
	}

	glGenFramebuffers(1, &shadowFBO);
	glGenTextures(1, &shadowMap);
	glBindTexture(GL_TEXTURE_2D, shadowMap);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24,
			   shadowMapSize, shadowMapSize, 0,
			   GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
	float borderColor[4] = {1.0f, 1.0f, 1.0f, 1.0f};
	glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

	glBindFramebuffer(GL_FRAMEBUFFER, shadowFBO);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadowMap, 0);
	glDrawBuffer(GL_NONE);
	glReadBuffer(GL_NONE);
	GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if (status != GL_FRAMEBUFFER_COMPLETE) {
		std::cerr << "[Shadow] FBO incomplete: 0x" << std::hex << status << std::dec << std::endl;
	}
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void StoneEngine::setShadowResolution(int newSize)
{
	shadowMapSize = std::max(256, newSize);
	rebuildShadowResources(RENDER_DISTANCE);
}

void StoneEngine::initSkybox()
{
	// std::cerr << "[Skybox] Init starting..." << std::endl;
	// First: single-file PNG (cross/strip/grid)
	{
		std::ifstream f(SKYBOX_SINGLE_PNG, std::ios::binary);
		if (f.is_open())
		{
			_hasSkybox = _skybox.loadFromSinglePNG(SKYBOX_SINGLE_PNG, /*fixSeams=*/true);
			// if (_hasSkybox) std::cerr << "[Skybox] Using single-file PNG: " << SKYBOX_SINGLE_PNG << std::endl;
		}
	}
	if (!_hasSkybox)
	{
		std::cerr << "[Skybox] No skybox loaded (PNG)." << std::endl;
	}
}

void StoneEngine::displaySun(FBODatas &targetFBO)
{
	if (showTriangleMesh)
		return;
	glBindFramebuffer(GL_FRAMEBUFFER, targetFBO.fbo);
	// glDepthFunc(GL_LESS);
	vec3 camPos = camera.getWorldPosition();
	// Compute current sun position based on time
	// Place the sun sprite far away in the current sun direction
	glm::vec3 sunDir = computeSunDirection(timeValue);
	glm::vec3 sunPos = camPos + sunDir * 6000.0f;

	// Update view matrix

	float radY, radX;
	radX = camera.getAngles().x * (M_PI / 180.0);
	radY = camera.getAngles().y * (M_PI / 180.0);

	mat4 sunViewMatrix = mat4(1.0f);
	sunViewMatrix = rotate(sunViewMatrix, radY, vec3(-1.0f, 0.0f, 0.0f));
	sunViewMatrix = rotate(sunViewMatrix, radX, vec3(0.0f, -1.0f, 0.0f));
	sunViewMatrix = translate(sunViewMatrix, vec3(camera.getPosition()));

	glUseProgram(sunShaderProgram);

	// Set uniforms
	glUniformMatrix4fv(glGetUniformLocation(sunShaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(sunViewMatrix));
	glUniformMatrix4fv(glGetUniformLocation(sunShaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projectionMatrix));
	glUniform3fv(glGetUniformLocation(sunShaderProgram, "sunPosition"), 1, glm::value_ptr(sunPos));

	glm::vec3 sunColor(1.0, 0.6, 0.2); // warm sun
	float height = clamp((sunPos.y - camPos.y) / 6000, 0.0f, 1.0f);
	sunColor = mix(sunColor, vec3(1.0f), height);
	glUniform3fv(glGetUniformLocation(sunShaderProgram, "sunColor"), 1, glm::value_ptr(sunColor));
	glUniform1f(glGetUniformLocation(sunShaderProgram, "intensity"), 1.0f);

	glEnable(GL_DEPTH_TEST); // allow occlusion by terrain
	glDepthMask(GL_FALSE);	 // prevent writing depth
	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE); // additive glow

	glBindVertexArray(sunVAO);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	glBindVertexArray(0);

	glDisable(GL_BLEND);
	glDepthMask(GL_TRUE);
	glCleanupTextureState();
}

void StoneEngine::initMsaaFramebuffers(FBODatas &fboData, int width, int height)
{
	// std::cout << "Samples available for MSAA: " << _maxSamples << std::endl;
	// Init MSAA framebuffer
	glGenFramebuffers(1, &fboData.fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, fboData.fbo);

	// Color renderbuffer
	glGenRenderbuffers(1, &fboData.texture);
	glBindRenderbuffer(GL_RENDERBUFFER, fboData.texture);
	glRenderbufferStorageMultisample(GL_RENDERBUFFER, _maxSamples, GL_RGB8, width, height);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, fboData.texture);

	// Depth renderbuffer
	glGenRenderbuffers(1, &fboData.depth);
	glBindRenderbuffer(GL_RENDERBUFFER, fboData.depth);
	glRenderbufferStorageMultisample(GL_RENDERBUFFER, _maxSamples, GL_DEPTH_COMPONENT24, width, height);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, fboData.depth);

	GLuint fboStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if (fboStatus != GL_FRAMEBUFFER_COMPLETE)
		std::cout << "Multisampled Framebuffer error: " << fboStatus << std::endl;

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void StoneEngine::initFramebuffers(FBODatas &fboData, int width, int height)
{
	// Init framebuffer
	glGenFramebuffers(1, &fboData.fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, fboData.fbo);

	// Init framebuffer color texture
	glGenTextures(1, &fboData.texture);
	glBindTexture(GL_TEXTURE_2D, fboData.texture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fboData.texture, 0);

	glGenTextures(1, &fboData.depth);
	glBindTexture(GL_TEXTURE_2D, fboData.depth);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	// Attach to framebuffer
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, fboData.depth, 0);

	GLuint fboStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if (fboStatus != GL_FRAMEBUFFER_COMPLETE)
		std::cout << "Framebuffer error: " << fboStatus << std::endl;
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

StoneEngine::PostProcessShader StoneEngine::createPostProcessShader(PostProcessShader &shader, const std::string &vertPath, const std::string &fragPath)
{
	glGenVertexArrays(1, &shader.vao);
	glGenBuffers(1, &shader.vbo);
	glBindVertexArray(shader.vao);
	glBindBuffer(GL_ARRAY_BUFFER, shader.vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(rectangleVertices), &rectangleVertices, GL_STATIC_DRAW);

	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)0);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)(2 * sizeof(float)));

	// Load shader
	shader.program = createShaderProgram(vertPath.c_str(), fragPath.c_str());
	glUseProgram(shader.program);
	glUniform1i(glGetUniformLocation(shader.program, "screenTexture"), 0);
	glUniform1i(glGetUniformLocation(shader.program, "depthTexture"), 1);

	updateFboWindowSize(shader);
	return shader;
}

void StoneEngine::initFboShaders()
{
	initFramebuffers(readFBO, windowWidth, windowHeight);
	initFramebuffers(writeFBO, windowWidth, windowHeight);
	initFramebuffers(tmpFBO, windowWidth, windowHeight);
	initMsaaFramebuffers(msaaFBO, windowWidth, windowHeight);
	createPostProcessShader(postProcessShaders[GREEDYFIX], "shaders/postProcess/postProcess.vert", "shaders/postProcess/greedyMeshing.frag");
	createPostProcessShader(postProcessShaders[FOG], "shaders/postProcess/postProcess.vert", "shaders/postProcess/fog.frag");
	createPostProcessShader(postProcessShaders[GODRAYS], "shaders/postProcess/postProcess.vert", "shaders/postProcess/lightshafts.frag");
	createPostProcessShader(
		postProcessShaders[CROSSHAIR],
		"shaders/postProcess/postProcess.vert",
		"shaders/postProcess/crosshair.frag");
	createPostProcessShader(postProcessShaders[SKYBOX_COMPOSITE],
							"shaders/postProcess/postProcess.vert",
							"shaders/postProcess/skyboxComposite.frag");
}

void StoneEngine::initDebugTextBox()
{
	vec3 *camPos = camera.getPositionPtr();
	fvec2 *camAngle = camera.getAnglesPtr();
	e_direction *facing_direction = camera.getDirectionPtr();
	float *yPos = camera.getYPtr();

	debugBox.initData(_window, 0, 0, 200, 200);
	debugBox.loadFont("textures/CASCADIAMONO.TTF", 20);
	debugBox.addLine("FPS: ", Textbox::DOUBLE, &fps);
	debugBox.addLine("Triangles: ", Textbox::INT, &drawnTriangles);
	debugBox.addLine("Chunk Memory: ", Textbox::SIZE_T, _chunkMgr.getMemorySizePtr());
	debugBox.addLine("RenderDistance: ", Textbox::INT, _chunkMgr.getRenderDistancePtr());
	debugBox.addLine("CurrentRender: ", Textbox::INT, _chunkMgr.getCurrentRenderPtr());
	debugBox.addLine("Chunks Cached: ", Textbox::INT, _chunkMgr.getCachedChunksCountPtr());
	debugBox.addLine("Chunks Displayed: ", Textbox::INT, _chunkMgr.getDisplayedChunksCountPtr());
	debugBox.addLine("Chunks Modified: ", Textbox::INT, _chunkMgr.getModifiedChunksCountPtr());
	debugBox.addLine("x: ", Textbox::FLOAT, &camPos->x);
	debugBox.addLine("y: ", Textbox::FLOAT, yPos);
	debugBox.addLine("z: ", Textbox::FLOAT, &camPos->z);
	debugBox.addLine("xangle: ", Textbox::FLOAT, &camAngle->x);
	debugBox.addLine("yangle: ", Textbox::FLOAT, &camAngle->y);
	debugBox.addLine("time: ", Textbox::INT, &timeValue);
	debugBox.addLine("Facing: ", Textbox::DIRECTION, facing_direction);
	debugBox.addLine("Biome: ", Textbox::BIOME, &_biome);
	debugBox.addLine("Temperature: ", Textbox::DOUBLE, &_temperature);
	debugBox.addLine("Humidity: ", Textbox::DOUBLE, &_humidity);
	debugBox.addLine("Selected block: ", Textbox::BLOCK, &selectedBlockDebug);

	// Nice soft sky blue
	glClearColor(0.53f, 0.81f, 0.92f, 1.0f);
}

void StoneEngine::initHelpTextBox()
{
	helpBox.initData(_window, 0, 0, 420, 420);
	helpBox.loadFont("textures/CASCADIAMONO.TTF", 20);
	helpBox.addStaticText("Help / Keybinds");
	helpBox.addStaticText("");
	helpBox.addStaticText("Esc: Quit");
	helpBox.addStaticText("");
	helpBox.addStaticText("W/A/S/D: Move");
	helpBox.addStaticText("Space: Jump");
	// Dynamic toggles below — values updated every frame
	_hGravity = "";
	_hGeneration = "";
	_hSprinting = "";
	_hUI = "";
	_hLighting = "";
	_hMouseCapture = "";
	_hDebug = "";
	_hHelp = "";
	_hWireframe = "";
	_hFullscreen = "";
	_empty = "";
	helpBox.addLine("Ctrl:  Sprinting ", Textbox::STRING, &_hSprinting);
	helpBox.addStaticText("");
	helpBox.addLine("F3:     Debug Overlay ", Textbox::STRING, &_hDebug);
	helpBox.addLine("H:      Help / Keybinds ", Textbox::STRING, &_hHelp);
	helpBox.addLine("F1:     UI ", Textbox::STRING, &_hUI);
	helpBox.addLine("F4:     Triangle Mesh ", Textbox::STRING, &_hWireframe);
	helpBox.addLine("F5:     Invert Camera", Textbox::STRING, &_empty); // no state; keep placeholder spacing
	helpBox.addLine("F11:    Fullscreen ", Textbox::STRING, &_hFullscreen);
	helpBox.addLine("G:      Gravity ", Textbox::STRING, &_hGravity);
	helpBox.addLine("L:      Lighting ", Textbox::STRING, &_hLighting);
	helpBox.addLine("M or ;: Mouse Capture ", Textbox::STRING, &_hMouseCapture);
	helpBox.addLine("C:      Generation ", Textbox::STRING, &_hGeneration);
	helpBox.addStaticText("");
	helpBox.addStaticText("Mouse Left:  Break block");
	helpBox.addStaticText("Mouse Right: Place block");
	helpBox.addStaticText("Mouse Middle: Pick block");
}

static inline const char *onoff(bool v) { return v ? "(On)" : "(Off)"; }

void StoneEngine::updateHelpStatusText()
{
	_hGravity = onoff(gravity);
	_hGeneration = onoff(updateChunk);
	_hSprinting = onoff(_player.isSprinting());
	_hUI = onoff(showUI);
	_hLighting = onoff(showLight);
	_hMouseCapture = onoff(mouseCaptureToggle);
	_hDebug = onoff(showDebugInfo);
	_hHelp = onoff(showHelp);
	_hWireframe = onoff(showTriangleMesh);
	_hFullscreen = onoff(_isFullscreen);
}

void StoneEngine::calculateFps()
{
	frameCount++;
	currentFrameTime = glfwGetTime();

	double timeInterval = currentFrameTime - lastFrameTime;

	if (timeInterval > 1)
	{
		fps = frameCount / timeInterval;

		lastFrameTime = currentFrameTime;
		frameCount = 0;

		std::stringstream title;
		title << "Not ft_minecraft | FPS: " << fps;
		glfwSetWindowTitle(_window, title.str().c_str());
	}
}

void StoneEngine::activateRenderShader()
{
	mat4 modelMatrix = mat4(1.0f);
	float radY, radX;
	radX = camera.getAngles().x * (M_PI / 180.0);
	radY = camera.getAngles().y * (M_PI / 180.0);

	// Build rotation-only view
	mat4 viewRot = mat4(1.0f);
	viewRot = rotate(viewRot, radY, vec3(-1.0f, 0.0f, 0.0f));
	viewRot = rotate(viewRot, radX, vec3(0.0f, -1.0f, 0.0f));

	// Build full view (with translation)
	mat4 viewFull = viewRot;
	viewFull = translate(viewFull, vec3(camera.getPosition()));

	this->viewMatrix = viewFull;
	_chunkMgr.setViewProj(this->viewMatrix, projectionMatrix);

	glUseProgram(shaderProgram);
	glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, value_ptr(projectionMatrix));
	glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, value_ptr(modelMatrix));
	// Pass rotation-only view to terrain shader (camera-relative positions in VS)
	glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, value_ptr(viewRot));
	glUniform3fv(glGetUniformLocation(shaderProgram, "lightColor"), 1, value_ptr(vec3(1.0f, 0.95f, 0.95f)));
	glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "lightSpaceMatrix"), 1, GL_FALSE, glm::value_ptr(lightSpaceMatrix));
	glUniform1f(glGetUniformLocation(shaderProgram, "shadowTexelWorld"), _shadowTexelWorld);
	glUniform1f(glGetUniformLocation(shaderProgram, "shadowNear"), _shadowNear);
	glUniform1f(glGetUniformLocation(shaderProgram, "shadowFar"), _shadowFar);
	glUniform1f(glGetUniformLocation(shaderProgram, "shadowBiasSlope"), _shadowBiasSlope);
	glUniform1f(glGetUniformLocation(shaderProgram, "shadowBiasConstant"), _shadowBiasConstant);

	// Pass camera world position explicitly for camera-relative rendering
	vec3 camWorld = camera.getWorldPosition();
	glUniform3fv(glGetUniformLocation(shaderProgram, "cameraPos"), 1, value_ptr(camWorld));

	// Provide a stable directional sun vector (camera-invariant)
	glm::vec3 sunDir = computeSunDirection(timeValue);
	glUniform3fv(glGetUniformLocation(shaderProgram, "sunDir"), 1, glm::value_ptr(sunDir));

	glActiveTexture(GL_TEXTURE4);
	glBindTexture(GL_TEXTURE_2D, shadowMap);
	glUniform1i(glGetUniformLocation(shaderProgram, "shadowMap"), 4);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D_ARRAY, _textureManager.getTextureArray());
	glUniform1i(glGetUniformLocation(shaderProgram, "textureArray"), 0);

	// Reactivate face culling for terrain (cull FRONT faces as requested)
	glEnable(GL_CULL_FACE);
	glCullFace(GL_FRONT);
	glFrontFace(GL_CCW);
}

void StoneEngine::renderShadowMap()
{
	if (!shadowShaderProgram) return;
	if (shadowMap == 0 || shadowFBO == 0)
		rebuildShadowResources(RENDER_DISTANCE);

	// ---------- Cached state: stabilize + avoid needless rebuilds ----------
	static bool      inited = false;
	static glm::vec3 cachedCenter(0.0f);
	static glm::vec3 prevSunDir(0.0f);
	static float     lastFov = -1.0f;
	static int       freezeFrames = 0;

	if (lastFov < 0.0f) lastFov = _fov;
	if (std::abs(_fov - lastFov) > 0.25f) {
		freezeFrames = 2;           // debounce FOV zoom changes
		lastFov = _fov;
	}
	if (freezeFrames > 0) { --freezeFrames; return; }

	// ---------- Sun dir depends only on time ----------
	glm::vec3 sunDir = glm::normalize(computeSunDirection(timeValue));

	// ---------- Grid-locked center near player (X/Z only) ----------
	const float GRID = float(CHUNK_SIZE) * 4.0f;  // snap every 4 chunks (tweak 2..8)
	glm::vec3 cam   = camera.getWorldPosition();
	glm::vec3 center(
		std::round(cam.x / GRID) * GRID,
		float(OCEAN_HEIGHT) + 32.0f,               // fixed Y anchor to avoid vertical jitter
		std::round(cam.z / GRID) * GRID
	);

	// ---------- Resolve world extent and quantize to texel steps ----------
	const float worldExtentRaw  = std::max<float>(128.0f, float(RENDER_DISTANCE * CHUNK_SIZE));
	const float halfWorldRaw    = worldExtentRaw;
	const float texelWorldRaw   = (2.0f * halfWorldRaw) / float(shadowMapSize);
	const float N               = 8.0f;                         // change extent only in N-texel steps
	const float step            = std::max(texelWorldRaw * N, 1.0f);
	const float halfWorld       = step * std::ceil(halfWorldRaw / step);

	// ---------- Place light & build stable matrices (single cascade) ----------
	const float shadowDist = std::max(halfWorld * 1.25f, 300.0f);
	glm::vec3  up(0.0f, 0.0f, 1.0f);                             // Z-up
	glm::vec3  lightPos = center + sunDir * shadowDist;
	glm::mat4  lightView = glm::lookAt(lightPos, center, up);

	_shadowNear = 1.0f;
	_shadowFar  = shadowDist + halfWorld;

	glm::mat4 lightProj = glm::ortho(-halfWorld, halfWorld,
									 -halfWorld, halfWorld,
									 _shadowNear, _shadowFar);

	// Texel snapping: snap the projected center to the shadow map pixel grid
	{
		glm::mat4 VP = lightProj * lightView;
		glm::vec4 origin = VP * glm::vec4(center, 1.0f);
		origin *= float(shadowMapSize) * 0.5f;                   // NDC->texel space
		glm::vec2 rounded = glm::floor(glm::vec2(origin) + 0.5f);
		glm::vec2 offset  = (rounded - glm::vec2(origin)) * (2.0f / float(shadowMapSize));
		lightProj[3][0] += offset.x;
		lightProj[3][1] += offset.y;
	}

	glm::mat4 newLightSpace = lightProj * lightView;
	const float newTexelWorld = (2.0f * halfWorld) / float(shadowMapSize);

	// ---------- Early-out: only update if center or sun changed enough ----------
	bool centerMoved = !inited || center.x != cachedCenter.x || center.z != cachedCenter.z;
	float cosDelta   = inited ? glm::clamp(glm::dot(glm::normalize(prevSunDir), sunDir), -1.0f, 1.0f) : 1.0f;
	float degDelta   = glm::degrees(std::acos(cosDelta));
	const float SUN_ANGLE_EPS = 0.25f;  // update only if sun rotated > 0.25°

	if (inited && !centerMoved && degDelta < SUN_ANGLE_EPS) {
		// Reuse previous shadow map contents without rebuilding this frame.
		return;
	}

	// Commit stabilized parameters
	inited             = true;
	cachedCenter       = center;
	prevSunDir         = sunDir;
	_shadowCenter      = center;
	lightSpaceMatrix   = newLightSpace;
	_shadowTexelWorld  = newTexelWorld;

	// ---------- Render the single shadow map ----------
	glUseProgram(shadowShaderProgram);
	glUniformMatrix4fv(glGetUniformLocation(shadowShaderProgram, "lightSpaceMatrix"), 1, GL_FALSE, glm::value_ptr(lightSpaceMatrix));
	glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_POLYGON_OFFSET_FILL);
	glPolygonOffset(3.0f, 6.0f);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D_ARRAY, _textureManager.getTextureArray());
	glUniform1i(glGetUniformLocation(shadowShaderProgram, "textureArray"), 0);

	// Shadow pass must NOT depend on screen-space occlusion
	_chunkMgr.setOcclusionSource(0, 0, 0, glm::mat4(1.0f), glm::mat4(1.0f), glm::vec3(0.0f));
	_chunkMgr.setRendererSyncMode(true);

	glViewport(0, 0, shadowMapSize, shadowMapSize);
	glBindFramebuffer(GL_FRAMEBUFFER, shadowFBO);
	glClear(GL_DEPTH_BUFFER_BIT);

	_chunkMgr.setViewProj(lightView, lightProj);

	// Solid geometry: cull FRONT faces in shadow pass as well
	glEnable(GL_CULL_FACE);
	glCullFace(GL_FRONT);
	glFrontFace(GL_CCW);
	_chunkMgr.renderSolidBlocks();
	glDisable(GL_POLYGON_OFFSET_FILL);
	_chunkMgr.renderTransparentBlocksNoCullForShadow();

	_chunkMgr.setRendererSyncMode(false);

	// ---------- Restore state ----------
	glDisable(GL_POLYGON_OFFSET_FILL);
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, windowWidth, windowHeight);
	glUseProgram(0);
}

void StoneEngine::renderChunkGrid()
{
	if (_gridMode == GRID_OFF || showTriangleMesh)
		return;

	// Snapshot chunk list
	std::vector<glm::ivec2> chunks;
	_chunkMgr.getDisplayedChunksSnapshot(chunks);
	if (chunks.empty())
		return;

	// Common state and matrices
	glUseProgram(_wireProgram);
	glBindVertexArray(_wireVAO);
	glEnable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glDepthMask(GL_FALSE); // test, don’t write
	glLineWidth(1.0f);

	// rotation-only view (same as terrain)
	float radY = camera.getAngles().y * (M_PI / 180.0f);
	float radX = camera.getAngles().x * (M_PI / 180.0f);
	glm::mat4 viewRot(1.0f);
	viewRot = glm::rotate(viewRot, radY, glm::vec3(-1, 0, 0));
	viewRot = glm::rotate(viewRot, radX, glm::vec3(0, -1, 0));

	glm::vec3 camW = camera.getWorldPosition();
	glUniformMatrix4fv(glGetUniformLocation(_wireProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projectionMatrix));
	glUniformMatrix4fv(glGetUniformLocation(_wireProgram, "view"), 1, GL_FALSE, glm::value_ptr(viewRot));
	glUniform3fv(glGetUniformLocation(_wireProgram, "cameraPos"), 1, glm::value_ptr(camW));
	glUniform1f(glGetUniformLocation(_wireProgram, "expand"), 0.003f);
	glUniform1f(glGetUniformLocation(_wireProgram, "depthBias"), 0.0008f);

	const float CS = float(CHUNK_SIZE);

	// Vertical window around camera, in subchunks
	float camY = camW.y;
	int camSY = int(std::floor(camY / CS));
	const int below = 2; // draw 2 subchunks below camera
	const int above = 5; // and 5 above
	int sy0 = camSY - below;
	int sy1 = camSY + above;

	// Colors
	GLint uColor = glGetUniformLocation(_wireProgram, "color");
	GLint uScale = glGetUniformLocation(_wireProgram, "scale");
	GLint uOff = glGetUniformLocation(_wireProgram, "worldOffset");

	for (const auto &cpos : chunks)
	{
		glm::vec3 base = glm::vec3(cpos.x * CS, 0.0f, cpos.y * CS);

		if (_gridMode == GRID_CHUNKS || _gridMode == GRID_BOTH)
		{
			float y0 = sy0 * CS;
			float spanY = float((sy1 - sy0 + 1)) * CS; // cover the vertical window with one tall box
			glUniform3f(uColor, 0.05f, 0.45f, 0.85f);  // chunk color (soft cyan)
			glUniform3f(uScale, CS, spanY, CS);
			glUniform3f(uOff, base.x, y0, base.z);
			glDrawArrays(GL_LINES, 0, 24);
		}

		if (_gridMode == GRID_SUBCHUNKS || _gridMode == GRID_BOTH)
		{
			glUniform3f(uColor, 0.90f, 0.35f, 0.70f); // subchunk color (magenta-ish)
			glUniform3f(uScale, CS, CS, CS);
			for (int sy = sy0; sy <= sy1; ++sy)
			{
				float y = sy * CS;
				glUniform3f(uOff, base.x, y, base.z);
				glDrawArrays(GL_LINES, 0, 24);
			}
		}
	}

	glDepthMask(GL_TRUE);
	glBindVertexArray(0);
}

void StoneEngine::activateTransparentShader()
{
	mat4 modelMatrix = mat4(1.0f);

	float radY, radX;
	radX = camera.getAngles().x * (M_PI / 180.0);
	radY = camera.getAngles().y * (M_PI / 180.0);

	// Rotation-only for rendering
	mat4 viewRot = mat4(1.0f);
	viewRot = rotate(viewRot, radY, vec3(-1.0f, 0.0f, 0.0f));
	viewRot = rotate(viewRot, radX, vec3(0.0f, -1.0f, 0.0f));

	// Full view for culling
	mat4 viewFull = viewRot;
	viewFull = translate(viewFull, vec3(camera.getPosition()));

	this->viewMatrix = viewFull;
	_chunkMgr.setViewProj(this->viewMatrix, projectionMatrix);

	vec3 viewPos = camera.getWorldPosition();

	glUseProgram(waterShaderProgram);

	// Matrices & camera
	glUniformMatrix4fv(glGetUniformLocation(waterShaderProgram, "projection"), 1, GL_FALSE, value_ptr(projectionMatrix));
	glUniformMatrix4fv(glGetUniformLocation(waterShaderProgram, "model"), 1, GL_FALSE, value_ptr(modelMatrix));
	glUniformMatrix4fv(glGetUniformLocation(waterShaderProgram, "view"), 1, GL_FALSE, value_ptr(viewRot));
	glUniformMatrix4fv(glGetUniformLocation(waterShaderProgram, "viewOpaque"), 1, GL_FALSE, value_ptr(viewFull));
	glUniform3fv(glGetUniformLocation(waterShaderProgram, "viewPos"), 1, value_ptr(viewPos));
	glUniform3fv(glGetUniformLocation(waterShaderProgram, "cameraPos"), 1, value_ptr(viewPos));
	glUniform3fv(glGetUniformLocation(waterShaderProgram, "lightColor"), 1, value_ptr(vec3(1.0f, 0.95f, 0.95f)));
	glUniform1f(glGetUniformLocation(waterShaderProgram, "time"), timeValue);
	glUniform1i(glGetUniformLocation(waterShaderProgram, "isUnderwater"), _player.isUnderWater());
	// In wireframe/triangle-mesh mode, force water to render flat deep blue with no reflections
	glUniform1i(glGetUniformLocation(waterShaderProgram, "showtrianglemesh"), showTriangleMesh ? 1 : 0);

	// Provide global water plane height to the water shader (for underwater depth-based effects)
	glUniform1f(glGetUniformLocation(waterShaderProgram, "waterHeight"), OCEAN_HEIGHT + 2);

	const float nearPlane = 0.1f;
	const float farPlane = FAR_PLANE;
	glUniform1f(glGetUniformLocation(waterShaderProgram, "nearPlane"), nearPlane);
	glUniform1f(glGetUniformLocation(waterShaderProgram, "farPlane"), farPlane);

	// Tweakables for Beer–Lambert absorption -> alpha
	glUniform1f(glGetUniformLocation(waterShaderProgram, "absorption"), 3.5f);
	glUniform1f(glGetUniformLocation(waterShaderProgram, "minAlpha"), 0.15f);
	glUniform1f(glGetUniformLocation(waterShaderProgram, "maxAlpha"), 0.95f);

	// TEXTURES
	// 0: screen color (opaque color buffer)
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, readFBO.texture);
	glUniform1i(glGetUniformLocation(waterShaderProgram, "screenTexture"), 0);

	// depth from opaque pass (sampled as regular 2D)
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, readFBO.depth);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glUniform1i(glGetUniformLocation(waterShaderProgram, "depthTexture"), 1);

	// 2: water normal map
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, waterNormalMap);
	glUniform1i(glGetUniformLocation(waterShaderProgram, "normalMap"), 2);
	// --- Planar reflection source (tmpFBO) ---
	{
		const float waterY = OCEAN_HEIGHT + 2.0f;
		float radY = camera.getAngles().y * (M_PI / 180.0f);
		float radX = camera.getAngles().x * (M_PI / 180.0f);

		glm::mat4 viewRotMirror(1.0f);
		viewRotMirror = glm::rotate(viewRotMirror, -radY, glm::vec3(-1.0f, 0.0f, 0.0f));
		viewRotMirror = glm::rotate(viewRotMirror, radX, glm::vec3(0.0f, -1.0f, 0.0f));

		glm::vec3 camW = camera.getWorldPosition();
		glm::vec3 camMir = glm::vec3(camW.x, 2.0f * waterY - camW.y, camW.z);

		glm::mat4 viewFullMirror = viewRotMirror;
		viewFullMirror = glm::translate(viewFullMirror, glm::vec3(-camMir.x, -camMir.y, -camMir.z));

		glm::vec4 planeWorld(0.0f, 1.0f, 0.0f, -waterY);
		glm::mat4 projOblique = makeObliqueProjection(projectionMatrix, viewFullMirror, planeWorld);

		glUniformMatrix4fv(glGetUniformLocation(waterShaderProgram, "planarView"), 1, GL_FALSE, glm::value_ptr(viewFullMirror));
		glUniformMatrix4fv(glGetUniformLocation(waterShaderProgram, "planarProjection"), 1, GL_FALSE, glm::value_ptr(projOblique));

		// --- Planar reflection source (tmpFBO) ---
		glActiveTexture(GL_TEXTURE3);
		glBindTexture(GL_TEXTURE_2D, tmpFBO.texture);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glUniform1i(glGetUniformLocation(waterShaderProgram, "planarTexture"), 3);
	}

	// Terrain atlas for non-water transparent faces (e.g., leaves)
	glActiveTexture(GL_TEXTURE4);
	glBindTexture(GL_TEXTURE_2D_ARRAY, _textureManager.getTextureArray());
	glUniform1i(glGetUniformLocation(waterShaderProgram, "textureArray"), 4);

	// Blending and depth settings for transparent pass
	if (showTriangleMesh)
	{
		// Wireframe view: draw all triangle edges clearly
		glDisable(GL_BLEND);
		glEnable(GL_DEPTH_TEST);
		glDepthMask(GL_TRUE);	 // write depth for proper line visibility
		glDisable(GL_CULL_FACE); // show both sides of water quads
		// Render water as actual wireframe lines
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	}
	else
	{
		// Water blended pass: depth write OFF
		glDepthMask(GL_FALSE);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glEnable(GL_DEPTH_TEST);
		glEnable(GL_CULL_FACE);
		glCullFace(GL_FRONT);
		glFrontFace(GL_CCW);
	}
}

void StoneEngine::initFlowerResources()
{
	// Create shader
	flowerProgram = createShaderProgram("shaders/render/flower.vert", "shaders/render/flower.frag");

	// Base X-shaped mesh: two vertical quads crossing at 90°
	// Each quad is two triangles, positions in meters within a unit block [0..1] height, centered at (0,0) on X/Z
	// width ~0.7 to avoid z-fighting
	const float W = 0.35f;
	const float H = 1.0f;
	// Interleaved: pos.xyz, uv
	float verts[] = {
		// Quad A (plane at Z=0) facing both sides (drawn with cull off)
		-W,
		0.0f,
		0.0f,
		0.0f,
		0.0f,
		W,
		0.0f,
		0.0f,
		1.0f,
		0.0f,
		W,
		H,
		0.0f,
		1.0f,
		1.0f,

		-W,
		0.0f,
		0.0f,
		0.0f,
		0.0f,
		W,
		H,
		0.0f,
		1.0f,
		1.0f,
		-W,
		H,
		0.0f,
		0.0f,
		1.0f,

		// Quad B (plane at X=0), rotated 90° around Y
		0.0f,
		0.0f,
		-W,
		0.0f,
		0.0f,
		0.0f,
		0.0f,
		W,
		1.0f,
		0.0f,
		0.0f,
		H,
		W,
		1.0f,
		1.0f,

		0.0f,
		0.0f,
		-W,
		0.0f,
		0.0f,
		0.0f,
		H,
		W,
		1.0f,
		1.0f,
		0.0f,
		H,
		-W,
		0.0f,
		1.0f,
	};

	glGenVertexArrays(1, &flowerVAO);
	glGenBuffers(1, &flowerVBO);
	glBindVertexArray(flowerVAO);
	glBindBuffer(GL_ARRAY_BUFFER, flowerVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);

	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *)0);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *)(3 * sizeof(float)));

	// Instance buffer: vec4 (xyz pos, rot), vec2 (scale, heightScale), int typeId
	glGenBuffers(1, &flowerInstanceVBO);
	glBindBuffer(GL_ARRAY_BUFFER, flowerInstanceVBO);
	glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);

	GLsizei fStride = (GLsizei)(sizeof(float) * 6 + sizeof(int));
	glEnableVertexAttribArray(2);
	glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, fStride, (void *)0);
	glVertexAttribDivisor(2, 1);
	glEnableVertexAttribArray(3);
	glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, fStride, (void *)(4 * sizeof(float)));
	glVertexAttribDivisor(3, 1);
	glEnableVertexAttribArray(4);
	glVertexAttribIPointer(4, 1, GL_INT, fStride, (void *)(6 * sizeof(float)));
	glVertexAttribDivisor(4, 1);

	glBindVertexArray(0);

	// Load flower texture from PNG with alpha (STB)
	glGenTextures(1, &flowerTexture);
	glBindTexture(GL_TEXTURE_2D_ARRAY, flowerTexture);
	std::vector<std::string> fileList = {
		"textures/flowers/poppy.png",
		"textures/flowers/dandelion.png",
		"textures/flowers/cyan_flower.png",
		"textures/flowers/short_grass.png",
		"textures/flowers/dead_bush.png"};
	// Keep only files that exist
	std::vector<std::string> files;
	for (const auto &f : fileList)
	{
		std::ifstream s(f, std::ios::binary);
		if (s.good())
			files.push_back(f);
	}
	if (files.empty())
	{
		std::cerr << "No flower textures found in textures/flowers" << std::endl;
	}
	const int nfiles = (int)files.size();
	int w = 0, h = 0, ch = 0;
	unsigned char *data = (nfiles > 0) ? stbi_load(files[0].c_str(), &w, &h, &ch, 4) : nullptr;
	if (data)
	{
		// Fallback: if no alpha present, carve background by edge flood-fill
		bool anyTransparent0 = false;
		for (int i = 0; i < w * h; ++i) if (data[4 * i + 3] < 255) { anyTransparent0 = true; break; }
		if (!anyTransparent0 && w > 1 && h > 1)
		{
			auto getPx = [&](int x, int y) -> glm::ivec3 { unsigned char* p = data + 4*(y*w + x); return {p[0],p[1],p[2]}; };
			glm::ivec3 corners[4] = {getPx(0,0), getPx(w-1,0), getPx(0,h-1), getPx(w-1,h-1)};
			glm::ivec3 key = corners[0];
			int best = 1;
			for (int i = 0; i < 4; ++i) {
				int cnt = 0; for (int j = 0; j < 4; ++j) if (glm::all(glm::equal(corners[i], corners[j]))) cnt++;
				if (cnt > best) { best = cnt; key = corners[i]; }
			}
			auto nearKey = [&](unsigned char r, unsigned char g, unsigned char b){
				int dr = int(r) - key.r; int dg = int(g) - key.g; int db = int(b) - key.b;
				return (abs(dr) + abs(dg) + abs(db)) <= 96; // generous threshold
			};
			std::vector<uint8_t> mark(w*h, 0);
			std::deque<std::pair<int,int>> q;
			auto push = [&](int x,int y){ if(x<0||y<0||x>=w||y>=h) return; int i=y*w+x; if(mark[i]) return; unsigned char* p=data+4*i; if(nearKey(p[0],p[1],p[2])){ mark[i]=1; q.emplace_back(x,y);} };
			for(int x=0;x<w;x++){ push(x,0); push(x,h-1);} for(int y=0;y<h;y++){ push(0,y); push(w-1,y);} 
			const int dx[8]={-1,0,1,-1,1,-1,0,1}; const int dy[8]={-1,-1,-1,0,0,1,1,1};
			while(!q.empty()){
				auto [x,y]=q.front(); q.pop_front();
				for(int k=0;k<8;k++){ int nx=x+dx[k], ny=y+dy[k]; if(nx<0||ny<0||nx>=w||ny>=h) continue; int idx=ny*w+nx; if(mark[idx]) continue; unsigned char* p=data+4*idx; if(nearKey(p[0],p[1],p[2])){ mark[idx]=1; q.emplace_back(nx,ny);} }
			}
			for(int i=0;i<w*h;i++) if(mark[i]){ unsigned char* p=data+4*i; p[0]=p[1]=p[2]=0; p[3]=0; }
		}
		// Zero RGB where fully transparent to avoid fringes
		for (int i = 0; i < w * h; ++i)
		{
			if (data[4 * i + 3] == 0)
			{
				data[4 * i + 0] = data[4 * i + 1] = data[4 * i + 2] = 0;
			}
		}
		glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8, w, h, nfiles, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
		glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, 0, w, h, 1, GL_RGBA, GL_UNSIGNED_BYTE, data);
		stbi_image_free(data);
		for (int layer = 1; layer < nfiles; ++layer)
		{
			int ww = 0, hh = 0, cc = 0;
			unsigned char *d = stbi_load(files[layer].c_str(), &ww, &hh, &cc, 4);
			if (!d)
				continue;
			// Fallback: add alpha via border flood-fill if needed
			bool anyTransparent = false;
			for (int i = 0; i < ww * hh; ++i) if (d[4 * i + 3] < 255) { anyTransparent = true; break; }
			if (!anyTransparent && ww > 1 && hh > 1)
			{
				auto getPx = [&](int x, int y) -> glm::ivec3 { unsigned char* p = d + 4*(y*ww + x); return {p[0],p[1],p[2]}; };
				glm::ivec3 corners[4] = {getPx(0,0), getPx(ww-1,0), getPx(0,hh-1), getPx(ww-1,hh-1)};
				glm::ivec3 key = corners[0];
				int best = 1; for(int i2=0;i2<4;i2++){ int cnt=0; for(int j=0;j<4;j++) if(glm::all(glm::equal(corners[i2], corners[j]))) cnt++; if(cnt>best){best=cnt; key=corners[i2];}}
				auto nearKey = [&](unsigned char r, unsigned char g, unsigned char b){ int dr=int(r)-key.r; int dg=int(g)-key.g; int db=int(b)-key.b; return (abs(dr)+abs(dg)+abs(db))<=96; };
				std::vector<uint8_t> mark(ww*hh,0);
				std::deque<std::pair<int,int>> q;
				auto push=[&](int x,int y){ if(x<0||y<0||x>=ww||y>=hh) return; int i=y*ww+x; if(mark[i]) return; unsigned char* p=d+4*i; if(nearKey(p[0],p[1],p[2])){ mark[i]=1; q.emplace_back(x,y);} };
				for(int x=0;x<ww;x++){ push(x,0); push(x,hh-1);} for(int y=0;y<hh;y++){ push(0,y); push(ww-1,y);} 
				const int dx[8]={-1,0,1,-1,1,-1,0,1}; const int dy[8]={-1,-1,-1,0,0,1,1,1};
				while(!q.empty()){
					auto [x,y]=q.front(); q.pop_front();
					for(int k=0;k<8;k++){ int nx=x+dx[k], ny=y+dy[k]; if(nx<0||ny<0||nx>=ww||ny>=hh) continue; int idx=ny*ww+nx; if(mark[idx]) continue; unsigned char* p=d+4*idx; if(nearKey(p[0],p[1],p[2])){ mark[idx]=1; q.emplace_back(nx,ny);} }
				}
				for(int i2=0;i2<ww*hh;i2++) if(mark[i2]){ unsigned char* p=d+4*i2; p[0]=p[1]=p[2]=0; p[3]=0; }
			}
			if (ww != w || hh != h)
			{
				std::vector<unsigned char> resized(w * h * 4);
				for (int y = 0; y < h; ++y)
				{
					for (int x = 0; x < w; ++x)
					{
						int sx = x * ww / w;
						int sy = y * hh / h;
						unsigned char *sp = d + 4 * (sy * ww + sx);
						unsigned char *dp = resized.data() + 4 * (y * w + x);
						dp[0] = sp[0];
						dp[1] = sp[1];
						dp[2] = sp[2];
						dp[3] = sp[3];
					}
				}
				for (int i = 0; i < w * h; ++i)
				{
					if (resized[4 * i + 3] == 0)
					{
						resized[4 * i + 0] = resized[4 * i + 1] = resized[4 * i + 2] = 0;
					}
				}
				glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, layer, w, h, 1, GL_RGBA, GL_UNSIGNED_BYTE, resized.data());
			}
			else
			{
				for (int i = 0; i < w * h; ++i)
				{
					if (d[4 * i + 3] == 0)
					{
						d[4 * i + 0] = d[4 * i + 1] = d[4 * i + 2] = 0;
					}
				}
				glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, layer, w, h, 1, GL_RGBA, GL_UNSIGNED_BYTE, d);
			}
			stbi_image_free(d);
		}
		glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		flowerLayerCount = nfiles;
		// Record layer indices by filename (robust to missing files)
		flowerShortGrassLayer = -1;
		_layerPoppy = _layerDandelion = _layerCyan = -1;
		_layerDeadBush = -1;
		for (int i = 0; i < nfiles; ++i)
		{
			const std::string &nm = files[i];
			if (nm.find("short_grass") != std::string::npos)
				flowerShortGrassLayer = i;
			else if (nm.find("poppy") != std::string::npos)
				_layerPoppy = i;
			else if (nm.find("dandelion") != std::string::npos)
				_layerDandelion = i;
			else if (nm.find("cyan_flower") != std::string::npos)
				_layerCyan = i;
			else if (nm.find("dead_bush") != std::string::npos)
				_layerDeadBush = i;
		}
	}
	else
	{
		std::cerr << "Failed to load flower texture array" << std::endl;
	}

	// Start empty; instances can be added at runtime
	flowerInstanceCount = 0;
	glBindBuffer(GL_ARRAY_BUFFER, flowerInstanceVBO);
	glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);
}
void StoneEngine::rebuildVisibleFlowersVBO()
{
	{
		std::vector<std::tuple<glm::ivec2, int, glm::ivec3, BlockType>> discovered;
		_chunkMgr.fetchAndClearDiscoveredFlowers(discovered);
		if (!discovered.empty())
		{
			static std::mt19937 rng{std::random_device{}()};
			std::uniform_real_distribution<float> rotD(-0.26f, 0.26f);
			std::uniform_real_distribution<float> scaD(0.95f, 1.05f);
			for (auto &t : discovered)
			{
				const glm::ivec2 cpos = std::get<0>(t);
				const int subY = std::get<1>(t);
				const glm::ivec3 cell = std::get<2>(t);
				const BlockType bt = std::get<3>(t);
				int typeId = (_layerPoppy >= 0) ? _layerPoppy : 0;
				if (bt == FLOWER_POPPY && _layerPoppy >= 0)
					typeId = _layerPoppy;
				else if (bt == FLOWER_DANDELION && _layerDandelion >= 0)
					typeId = _layerDandelion;
				else if (bt == FLOWER_CYAN && _layerCyan >= 0)
					typeId = _layerCyan;
				else if (bt == FLOWER_SHORT_GRASS && flowerShortGrassLayer >= 0)
					typeId = flowerShortGrassLayer;
				else if (bt == FLOWER_DEAD_BUSH && _layerDeadBush >= 0)
					typeId = _layerDeadBush;
				glm::vec3 center(cell.x + 0.5f, cell.y + 0.0f, cell.z + 0.5f);
				// Poppy can appear slightly above ground; nudge it down a bit
				if (bt == FLOWER_POPPY)
					center.y -= 0.1f;
				FlowerInstance inst{center, rotD(rng), scaD(rng), 1.0f, typeId};
				_flowersBySub[cpos][subY].push_back(inst);
			}
		}
	}
	std::vector<glm::ivec2> chunks;
	_chunkMgr.getDisplayedChunksSnapshot(chunks);
	std::unordered_map<glm::ivec2, std::unordered_set<int>, ivec2_hash> visibleSub;
	_chunkMgr.getDisplayedSubchunksSnapshot(visibleSub);

	// Prune cached flower instances for chunks that are no longer displayed
	// to avoid unbounded memory growth when roaming far.
	if (!chunks.empty())
	{
		std::unordered_set<glm::ivec2, ivec2_hash> keep;
		keep.reserve(chunks.size());
		for (const auto &c : chunks) keep.insert(c);
		for (auto it = _flowersBySub.begin(); it != _flowersBySub.end(); )
		{
			if (keep.find(it->first) == keep.end()) it = _flowersBySub.erase(it);
			else ++it;
		}
	}
	_visibleFlowers.clear();
	for (const auto &c : chunks)
	{
		auto it = _flowersBySub.find(c);
		if (it == _flowersBySub.end())
			continue;
		auto visIt = visibleSub.find(c);
		// Only render flowers for sublayers that are actually displayed.
		// If there is no snapshot yet for this chunk, skip for now to avoid
		// flowers appearing before terrain meshes.
		if (visIt == visibleSub.end() || visIt->second.empty())
			continue;
		const auto &allowed = visIt->second;
		for (auto &kv : it->second)
		{
			int subY = kv.first;
			if (allowed.find(subY) == allowed.end())
				continue;
			auto &vec = kv.second;
			_visibleFlowers.insert(_visibleFlowers.end(), vec.begin(), vec.end());
		}
	}
	flowerInstanceCount = (GLsizei)_visibleFlowers.size();
	std::vector<unsigned char> buffer;
	buffer.resize(_visibleFlowers.size() * (sizeof(float) * 6 + sizeof(int)));
	unsigned char *ptr = buffer.data();
	for (const auto &f : _visibleFlowers)
	{
		float tmp[6] = {f.pos.x, f.pos.y, f.pos.z, f.rot, f.scale, f.heightScale};
		memcpy(ptr, tmp, sizeof(tmp));
		ptr += sizeof(tmp);
		memcpy(ptr, &f.typeId, sizeof(int));
		ptr += sizeof(int);
	}
	glBindBuffer(GL_ARRAY_BUFFER, flowerInstanceVBO);
	glBufferData(GL_ARRAY_BUFFER, buffer.size(), buffer.data(), GL_DYNAMIC_DRAW);
}

void StoneEngine::renderFlowers()
{
	if (showTriangleMesh)
		return; // skip in wireframe mode
	if (!flowerProgram || flowerVAO == 0)
		return;
	rebuildVisibleFlowersVBO();
	if (flowerInstanceCount == 0)
		return;

	glUseProgram(flowerProgram);

	// Build rotation-only view (same convention as terrain/water)
	float radY = camera.getAngles().y * (M_PI / 180.0f);
	float radX = camera.getAngles().x * (M_PI / 180.0f);
	glm::mat4 viewRot(1.0f);
	viewRot = glm::rotate(viewRot, radY, glm::vec3(-1.0f, 0.0f, 0.0f));
	viewRot = glm::rotate(viewRot, radX, glm::vec3(0.0f, -1.0f, 0.0f));

	glUniformMatrix4fv(glGetUniformLocation(flowerProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projectionMatrix));
	glUniformMatrix4fv(glGetUniformLocation(flowerProgram, "view"), 1, GL_FALSE, glm::value_ptr(viewRot));
	glUniformMatrix4fv(glGetUniformLocation(flowerProgram, "model"), 1, GL_FALSE, glm::value_ptr(glm::mat4(1.0f)));
	glUniform3fv(glGetUniformLocation(flowerProgram, "cameraPos"), 1, glm::value_ptr(camera.getWorldPosition()));
	glUniform1f(glGetUniformLocation(flowerProgram, "time"), (float)glfwGetTime());

	glActiveTexture(GL_TEXTURE3);
	glBindTexture(GL_TEXTURE_2D_ARRAY, flowerTexture);
	glUniform1i(glGetUniformLocation(flowerProgram, "flowerTex"), 3);
	glUniform1i(glGetUniformLocation(flowerProgram, "shortGrassLayer"), flowerShortGrassLayer);
	glUniform1i(glGetUniformLocation(flowerProgram, "timeValue"), timeValue);
	glUniform3f(glGetUniformLocation(flowerProgram, "lightColor"), 1.0f, 0.95f, 0.95f);

	// State: cutout alpha, no blending, depth test/write on, culling off
	glDisable(GL_BLEND);
	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
	glDisable(GL_CULL_FACE);

	glBindVertexArray(flowerVAO);
	glDrawArraysInstanced(GL_TRIANGLES, 0, 12, flowerInstanceCount);
	glBindVertexArray(0);
	glEnable(GL_CULL_FACE);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
	glCleanupTextureState();
}

void StoneEngine::addFlower(glm::vec3 pos, int typeId, float rotJitter, float scale, float heightScale)
{
	FlowerInstance inst{pos, rotJitter, scale, heightScale, typeId};
	// Compute owning chunk and subchunk layer
	glm::ivec2 cpos(
		(int)std::floor(pos.x / (float)CHUNK_SIZE),
		(int)std::floor(pos.z / (float)CHUNK_SIZE));
	int subY = (int)std::floor(pos.y / (float)CHUNK_SIZE);
	_flowersBySub[cpos][subY].push_back(inst);
}

void StoneEngine::removeFlowerAtCell(const glm::ivec3 &cell)
{
	glm::ivec2 cpos(
		(int)std::floor(cell.x / (float)CHUNK_SIZE),
		(int)std::floor(cell.z / (float)CHUNK_SIZE));
	int subY = (int)std::floor(cell.y / (float)CHUNK_SIZE);
	auto itC = _flowersBySub.find(cpos);
	if (itC == _flowersBySub.end())
		return;
	auto itS = itC->second.find(subY);
	if (itS == itC->second.end())
		return;
	glm::vec3 center(cell.x + 0.5f, cell.y + 0.0f, cell.z + 0.5f);
	auto &vec = itS->second;
	vec.erase(std::remove_if(vec.begin(), vec.end(), [&](const FlowerInstance &f)
							 { return (fabs(f.pos.x - center.x) < 0.51f && fabs(f.pos.z - center.z) < 0.51f && fabs(f.pos.y - center.y) < 0.51f); }),
			  vec.end());
}

void StoneEngine::blitColor(FBODatas &src, FBODatas &dst)
{
	glBindFramebuffer(GL_READ_FRAMEBUFFER, src.fbo);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, dst.fbo);
	glBlitFramebuffer(0, 0, windowWidth, windowHeight, 0, 0, windowWidth, windowHeight,
					  GL_COLOR_BUFFER_BIT, GL_NEAREST);
}

void StoneEngine::blitColorDepth(FBODatas &src, FBODatas &dst)
{
	glBindFramebuffer(GL_READ_FRAMEBUFFER, src.fbo);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, dst.fbo);
	glBlitFramebuffer(0, 0, windowWidth, windowHeight, 0, 0, windowWidth, windowHeight,
					  GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT, GL_NEAREST);
}

void StoneEngine::resolveMsaaToFbo(FBODatas &dst, bool copyDepth)
{
	glBindFramebuffer(GL_READ_FRAMEBUFFER, msaaFBO.fbo);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, dst.fbo);

	GLbitfield mask = GL_COLOR_BUFFER_BIT;
	if (copyDepth)
		mask |= GL_DEPTH_BUFFER_BIT;
	glBlitFramebuffer(0, 0, windowWidth, windowHeight,
					  0, 0, windowWidth, windowHeight,
					  mask, GL_NEAREST);
}
void StoneEngine::display()
{
	// If no chunks are visible yet, show a simple loading screen
	{
		if (!_chunkMgr.hasRenderableChunks() || std::chrono::steady_clock::now() < _splashDeadline)
		{
			renderLoadingScreen();
			return;
		}
	}

	// Pass 0: shadow map
	renderShadowMap();

	prepareRenderPipeline();

	renderSkybox();		  // -> msaaFBO
	renderSolidObjects(); // -> msaaFBO

	// Cutout flowers pass (after opaque, before water/transparents)
	glBindFramebuffer(GL_FRAMEBUFFER, msaaFBO.fbo);
	renderFlowers();

	// Resolve OPAQUE
	resolveMsaaToFbo(writeFBO, true);
	blitColorDepth(writeFBO, readFBO);

	displaySun(writeFBO);
	blitColor(writeFBO, readFBO);

	renderPlanarReflection();

	// TRANSPARENT
	if (!showTriangleMesh)
	{
		glBindFramebuffer(GL_FRAMEBUFFER, msaaFBO.fbo);
		renderTransparentObjects();
		renderAimHighlight();
		renderChunkGrid();

		resolveMsaaToFbo(writeFBO, /*copyDepth=*/true);
		blitColorDepth(writeFBO, readFBO);
	}
	else
	{
		// In wireframe mode, previous blits changed GL_DRAW_FRAMEBUFFER.
	// Render lines to the default framebuffer for visibility.
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		renderTransparentObjects(); // direct
	}

	// Run god rays BEFORE greedy fix so depth still marks sky properly
	postProcessGodRays();
	blitColor(writeFBO, readFBO);

	// Greedy fix (needs depth, may WRITE gl_FragDepth)
	postProcessGreedyFix();
	blitColorDepth(writeFBO, readFBO);

	// Fog
	postProcessFog();
	blitColor(writeFBO, readFBO);
	postProcessSkyboxComposite();
	blitColor(writeFBO, readFBO);

	displaySun(writeFBO);
	blitColor(writeFBO, readFBO);

	if (showUI)
	{
		postProcessCrosshair();
		blitColor(writeFBO, readFBO);
	}

	sendPostProcessFBOToDispay(writeFBO);
	renderOverlayAndUI();
	finalizeFrame();

	// Save current camera/view/proj for next frame's occlusion reprojection
	_prevViewOcc = viewMatrix;
	_prevProjOcc = projectionMatrix;
	_prevCamOcc  = camera.getWorldPosition();
	_prevOccValid = true;
	glCleanupTextureState();
}

void StoneEngine::renderLoadingScreen()
{
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glDisable(GL_DEPTH_TEST);
	glClearColor(0.07f, 0.09f, 0.12f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	if (!_loadingInit)
	{
		_loadingBox.initData(_window, 0, 0, windowWidth, windowHeight);
		_loadingBox.loadFont("textures/CASCADIAMONO.TTF", 36);
		_loadingBox.addStaticText(_loadingText);
		_loadingInit = true;
	}

	// Center text by adjusting its starting position roughly to middle
	// Textbox renders relative to internal offsets; re-init dimensions on resize
	_loadingBox.initData(_window, windowWidth / 2 - 90, windowHeight / 2 - 18, windowWidth, windowHeight);
	_loadingBox.render();
	glfwSwapBuffers(_window);
}

void StoneEngine::postProcessSkyboxComposite()
{
	if (showTriangleMesh)
		return;

	PostProcessShader &shader = postProcessShaders[SKYBOX_COMPOSITE];
	glBindFramebuffer(GL_FRAMEBUFFER, writeFBO.fbo);
	glUseProgram(shader.program);
	glBindVertexArray(shader.vao);
	glDisable(GL_DEPTH_TEST);
	glDepthMask(GL_FALSE);

	// Inputs: scene color and depth
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, readFBO.texture);
	glUniform1i(glGetUniformLocation(shader.program, "screenTexture"), 0);

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, readFBO.depth);
	glUniform1i(glGetUniformLocation(shader.program, "depthTexture"), 1);

	// Cubemap
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_CUBE_MAP, _skybox.getTextureID());
	glUniform1i(glGetUniformLocation(shader.program, "skybox"), 2);

	// Matrices and params
	glm::mat4 invProj = glm::inverse(projectionMatrix);
	glUniformMatrix4fv(glGetUniformLocation(shader.program, "invProjection"), 1, GL_FALSE, glm::value_ptr(invProj));
	// Build rotation-only view and pass its inverse (transpose for pure rotation)
	float radY = camera.getAngles().y * (M_PI / 180.0f);
	float radX = camera.getAngles().x * (M_PI / 180.0f);
	glm::mat4 viewRot(1.0f);
	viewRot = glm::rotate(viewRot, radY, glm::vec3(-1.0f, 0.0f, 0.0f));
	viewRot = glm::rotate(viewRot, radX, glm::vec3(0.0f, -1.0f, 0.0f));
	glm::mat3 invViewRot = glm::transpose(glm::mat3(viewRot));
	glUniformMatrix3fv(glGetUniformLocation(shader.program, "invViewRot"), 1, GL_FALSE, glm::value_ptr(invViewRot));
	// Treat only truly empty pixels (cleared depth ~= 1.0) as sky; keep far terrain
	glUniform1f(glGetUniformLocation(shader.program, "depthThreshold"), 0.9999999f);
	glUniform1i(glGetUniformLocation(shader.program, "useLod0"), 1);
	glUniform1f(glGetUniformLocation(shader.program, "edgeBias"), 0.998f);

	glDrawArrays(GL_TRIANGLES, 0, 6);

	glDepthMask(GL_TRUE);
	glCleanupTextureState();
}

void StoneEngine::renderAimHighlight()
{
	if (showTriangleMesh)
		return;

	// Raycast
	glm::ivec3 hit;
	if (!_chunkMgr.raycastHit(camera.getWorldPosition(), camera.getDirection(), 5.0f, hit))
		return;

	// Determine block type at hit to adapt highlight bbox (logs are visually inset)
	glm::ivec2 hitChunkPos(
		(int)std::floor((float)hit.x / (float)CHUNK_SIZE),
		(int)std::floor((float)hit.z / (float)CHUNK_SIZE));
	BlockType hitBlock = _chunkMgr.getBlock(hitChunkPos, hit);

	// Default: full block
	glm::vec3 bboxOffset = glm::vec3(hit);
	glm::vec3 bboxScale = glm::vec3(1.0f, 1.0f, 1.0f);

	// Match the visual inset used in shaders/render/terrain.vert (LOG_INSET = 0.10)
	if (hitBlock == LOG || hitBlock == CACTUS)
	{
		const float inset = 0.10f;
		bboxOffset.x += inset;
		bboxOffset.z += inset;
		bboxScale.x = 1.0f - 2.0f * inset;
		bboxScale.z = 1.0f - 2.0f * inset;
	}
	// Decorative plants (flowers/short grass) render as X-cross quads of width ~0.70m.
	// Adapt the aim highlight to their smaller footprint so the wireframe hugs the plant.
	else if (hitBlock == FLOWER_POPPY || hitBlock == FLOWER_DANDELION ||
			 hitBlock == FLOWER_CYAN  || hitBlock == FLOWER_SHORT_GRASS ||
			 hitBlock == FLOWER_DEAD_BUSH)
	{
		// Base mesh half-width W = 0.35 (see initFlowerResources), so baseline total ~0.70.
		// Cyan sprite is visually slimmer; match poppy/dandelion to cyan width.
		float plantScaleXZ = 0.70f;
		const float cyanScaleXZ = 0.60f;
		// Use cyan-sized footprint for cyan, poppy, dandelion and dead bush
		if (hitBlock == FLOWER_CYAN || hitBlock == FLOWER_POPPY || hitBlock == FLOWER_DANDELION || hitBlock == FLOWER_DEAD_BUSH)
			plantScaleXZ = cyanScaleXZ;
		const float inset = (1.0f - plantScaleXZ) * 0.5f; // center within the voxel
		bboxOffset.x += inset;
		bboxOffset.z += inset;
		bboxScale.x = plantScaleXZ;
		bboxScale.z = plantScaleXZ;
		// Adjust height per plant type for aim wireframe only
		if (hitBlock == FLOWER_POPPY || hitBlock == FLOWER_DANDELION)
		{
			// A bit taller than half of cyan's (baseline) height
			bboxScale.y = 0.65f;
		}
		// Short grass: make wireframe almost a full block tall for visibility
		if (hitBlock == FLOWER_SHORT_GRASS)
		{
			// Make the wireframe almost a full block tall for visibility
			bboxScale.y = 0.95f;
		}
	}

	// Setup state
	glUseProgram(_wireProgram);
	glBindVertexArray(_wireVAO);
	glEnable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glLineWidth(2.0f);

	// Same transforms as terrain: projection + rotation-only view
	float radY = camera.getAngles().y * (M_PI / 180.0f);
	float radX = camera.getAngles().x * (M_PI / 180.0f);
	glm::mat4 viewRot(1.0f);
	viewRot = glm::rotate(viewRot, radY, glm::vec3(-1.0f, 0.0f, 0.0f));
	viewRot = glm::rotate(viewRot, radX, glm::vec3(0.0f, -1.0f, 0.0f));

	glUniformMatrix4fv(glGetUniformLocation(_wireProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projectionMatrix));
	glUniformMatrix4fv(glGetUniformLocation(_wireProgram, "view"), 1, GL_FALSE, glm::value_ptr(viewRot));
	glm::vec3 camW = camera.getWorldPosition();
	glUniform3fv(glGetUniformLocation(_wireProgram, "cameraPos"), 1, glm::value_ptr(camW));
	glUniform3fv(glGetUniformLocation(_wireProgram, "worldOffset"), 1, glm::value_ptr(bboxOffset));
	glUniform3f(glGetUniformLocation(_wireProgram, "color"), 0.06f, 0.06f, 0.06f);
	glUniform1f(glGetUniformLocation(_wireProgram, "expand"), 0.003f);	   // ~3 mm at 1m/unit
	glUniform1f(glGetUniformLocation(_wireProgram, "depthBias"), 0.0008f); // tiny, but effective
	glUniform3f(glGetUniformLocation(_wireProgram, "scale"), bboxScale.x, bboxScale.y, bboxScale.z);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);
	// Test against scene depth; avoid depth writes to preserve later passes
	glDepthMask(GL_FALSE);

	glBindVertexArray(_wireVAO);
	glLineWidth(2.0f);
	glDrawArrays(GL_LINES, 0, 24);
	glDepthMask(GL_TRUE);
}

void StoneEngine::postProcessCrosshair()
{
	if (showTriangleMesh)
		return;

	PostProcessShader &shader = postProcessShaders[CROSSHAIR];

	glBindFramebuffer(GL_FRAMEBUFFER, writeFBO.fbo);
	glUseProgram(shader.program);
	glBindVertexArray(shader.vao);
	glDisable(GL_DEPTH_TEST);
	glDepthMask(GL_FALSE);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, readFBO.texture);
	glUniform1i(glGetUniformLocation(shader.program, "screenTexture"), 0);

	glDrawArrays(GL_TRIANGLES, 0, 6);

	glCleanupTextureState();
}

void StoneEngine::postProcessFog()
{
	if (showTriangleMesh)
		return;

	PostProcessShader &shader = postProcessShaders[FOG];
	glBindFramebuffer(GL_FRAMEBUFFER, writeFBO.fbo);
	glUseProgram(shader.program);
	glBindVertexArray(shader.vao);
	glDisable(GL_DEPTH_TEST);
	glDepthMask(GL_FALSE);

	// Inputs
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, readFBO.texture);
	glUniform1i(glGetUniformLocation(shader.program, "screenTexture"), 0);

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, readFBO.depth);
	glUniform1i(glGetUniformLocation(shader.program, "depthTexture"), 1);

	glUniform1f(glGetUniformLocation(shader.program, "nearPlane"), NEAR_PLANE);
	glUniform1f(glGetUniformLocation(shader.program, "farPlane"), FAR_PLANE);
	glUniform1f(glGetUniformLocation(shader.program, "skyDepthThreshold"), 0.9999999f);
	glUniform3f(glGetUniformLocation(shader.program, "fogColor"),
				0.46f, 0.49f, 0.52f);

	glUniform1i(glGetUniformLocation(shader.program, "isUnderwater"), _player.isUnderWater());
	glUniform1f(glGetUniformLocation(shader.program, "waterHeight"), OCEAN_HEIGHT + 2);
	glUniform3fv(glGetUniformLocation(shader.program, "viewPos"), 1, glm::value_ptr(camera.getWorldPosition()));

	GLint loc = glGetUniformLocation(shader.program, "renderDistance");
	if (auto render = _chunkMgr.getCurrentRenderPtr(); render)
	{
		_bestRender = std::max(_bestRender, *render);
		glUniform1f(loc, _bestRender - 5);
	}
	else
	{
		glUniform1f(loc, RENDER_DISTANCE);
	}

	glClear(GL_COLOR_BUFFER_BIT);
	glDrawArrays(GL_TRIANGLES, 0, 6);

	glDepthMask(GL_TRUE);
	glCleanupTextureState();
}

void StoneEngine::postProcessGreedyFix()
{
	if (showTriangleMesh)
		return;

	PostProcessShader &shader = postProcessShaders[GREEDYFIX];

	glBindFramebuffer(GL_FRAMEBUFFER, writeFBO.fbo);
	glUseProgram(shader.program);
	glBindVertexArray(shader.vao);
	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
	glDepthFunc(GL_ALWAYS);

	// Bind screen texture (color)
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, readFBO.texture);
	glUniform1i(glGetUniformLocation(shader.program, "screenTexture"), 0);

	// Bind depth texture
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, readFBO.depth); // If shared, keep using dboTexture
	glUniform1i(glGetUniformLocation(shader.program, "depthTexture"), 1);

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	glDepthFunc(GL_LESS);
	glCleanupTextureState();
}

void StoneEngine::postProcessGodRays()
{
	if (showTriangleMesh)
		return;

	PostProcessShader &shader = postProcessShaders[GODRAYS];

	glBindFramebuffer(GL_FRAMEBUFFER, writeFBO.fbo);
	glUseProgram(shader.program);
	glBindVertexArray(shader.vao);
	glDisable(GL_DEPTH_TEST);
	glDepthMask(GL_FALSE);

	// Inputs: scene color and depth from readFBO
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, readFBO.texture);
	glUniform1i(glGetUniformLocation(shader.program, "screenTexture"), 0);

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, readFBO.depth);
	glUniform1i(glGetUniformLocation(shader.program, "depthTexture"), 1);

	// View matrix built from camera
	glm::mat4 viewMatrix = glm::mat4(1.0f);
	float radX = camera.getAngles().x * (M_PI / 180.0f);
	float radY = camera.getAngles().y * (M_PI / 180.0f);
	viewMatrix = glm::rotate(viewMatrix, radY, glm::vec3(-1.0f, 0.0f, 0.0f));
	viewMatrix = glm::rotate(viewMatrix, radX, glm::vec3(0.0f, -1.0f, 0.0f));
	viewMatrix = glm::translate(viewMatrix, camera.getPosition());

	vec3 camPos = camera.getWorldPosition();
	// Match the same far distance used by the sun sprite so positions coincide
	glm::vec3 sunPos = camPos + computeSunDirection(timeValue) * 6000.0f;

	// Uniforms
	glUniformMatrix4fv(glGetUniformLocation(shader.program, "view"), 1, GL_FALSE, glm::value_ptr(viewMatrix));
	glUniformMatrix4fv(glGetUniformLocation(shader.program, "projection"), 1, GL_FALSE, glm::value_ptr(projectionMatrix));
	glUniform3fv(glGetUniformLocation(shader.program, "sunPos"), 1, glm::value_ptr(sunPos));
	glUniform1i(glGetUniformLocation(shader.program, "isUnderwater"), _player.isUnderWater());

	// Intensity scales with sun altitude and underwater state
	float sunHeight = glm::clamp((sunPos.y - camPos.y) / 6000.0f, 0.0f, 1.0f);
	// Boost underwater so rays are more visible while submerged, avoid overexposure
	float baseIntensity = _player.isUnderWater() ? 1.25f : 0.60f;
	float rayIntensity = baseIntensity * sunHeight;
	glUniform1f(glGetUniformLocation(shader.program, "rayIntensity"), rayIntensity);

	glClear(GL_COLOR_BUFFER_BIT);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
	glCleanupTextureState();
}

void StoneEngine::renderPlanarReflection()
{
	if (showTriangleMesh)
		return;

	const float waterY = OCEAN_HEIGHT + 2.0f;
	glm::vec3 camPos = camera.getWorldPosition();
	const float verticalDistance = glm::abs(camPos.y - waterY);
	const float planarUpdateMaxDistance = 256.0f;

	if (!_player.isUnderWater() && verticalDistance > planarUpdateMaxDistance)
	{
		return;
	}

	glBindFramebuffer(GL_FRAMEBUFFER, tmpFBO.fbo);
	glViewport(0, 0, windowWidth, windowHeight);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	float radY = camera.getAngles().y * (M_PI / 180.0f);
	float radX = camera.getAngles().x * (M_PI / 180.0f);

	glm::mat4 viewRotMirror(1.0f);
	viewRotMirror = glm::rotate(viewRotMirror, -radY, glm::vec3(-1.0f, 0.0f, 0.0f));
	viewRotMirror = glm::rotate(viewRotMirror, radX, glm::vec3(0.0f, -1.0f, 0.0f));

	glm::vec3 camMir = glm::vec3(camPos.x, 2.0f * waterY - camPos.y, camPos.z);

	glm::mat4 viewFullMirror = viewRotMirror;
	viewFullMirror = glm::translate(viewFullMirror, glm::vec3(-camMir.x, -camMir.y, -camMir.z));

	// Plane in world space: 0*x + 1*y + 0*z - waterY = 0
	glm::vec4 planeWorld(0.0f, 1.0f, 0.0f, -waterY);
	glm::mat4 projOblique = makeObliqueProjection(projectionMatrix, viewFullMirror, planeWorld);

	// 0) Skybox into planar FBO so reflections include sky
	if (_hasSkybox && skyboxProgram != 0)
	{
		glUseProgram(skyboxProgram);
		glUniformMatrix4fv(glGetUniformLocation(skyboxProgram, "view"), 1, GL_FALSE, glm::value_ptr(viewRotMirror));
		glUniformMatrix4fv(glGetUniformLocation(skyboxProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projOblique));
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_CUBE_MAP, _skybox.getTextureID());
		glUniform1i(glGetUniformLocation(skyboxProgram, "skybox"), 0);
		GLboolean cullSkyEnabled = glIsEnabled(GL_CULL_FACE);
		glDisable(GL_CULL_FACE);
		_skybox.render();
		if (cullSkyEnabled)
			glEnable(GL_CULL_FACE);
	}

	glUseProgram(shaderProgram);
	glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projOblique));
	glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(viewRotMirror));
	glUniform3fv(glGetUniformLocation(shaderProgram, "cameraPos"), 1, glm::value_ptr(camMir));

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D_ARRAY, _textureManager.getTextureArray());
	glUniform1i(glGetUniformLocation(shaderProgram, "textureArray"), 0);

	GLboolean cullWasEnabled = glIsEnabled(GL_CULL_FACE);
	if (cullWasEnabled)
		glDisable(GL_CULL_FACE);

	glm::mat4 prevView = this->viewMatrix;
	_chunkMgr.setViewProj(viewFullMirror, projOblique); // <- use oblique for culling too
	_chunkMgr.updateDrawData();
	_chunkMgr.renderSolidBlocks();

	// Also render masked alpha (leaves) into planar reflection
	glUseProgram(alphaShaderProgram);
	glUniformMatrix4fv(glGetUniformLocation(alphaShaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projOblique));
	glUniformMatrix4fv(glGetUniformLocation(alphaShaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(viewRotMirror));
	glUniformMatrix4fv(glGetUniformLocation(alphaShaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(glm::mat4(1.0f)));
	glUniform3fv(glGetUniformLocation(alphaShaderProgram, "cameraPos"), 1, glm::value_ptr(camMir));
	glUniform3fv(glGetUniformLocation(alphaShaderProgram, "lightColor"), 1, glm::value_ptr(glm::vec3(1.0f, 0.95f, 0.95f)));
	glUniform1f(glGetUniformLocation(alphaShaderProgram, "time"), (float)glfwGetTime());
	glUniform1i(glGetUniformLocation(alphaShaderProgram, "timeValue"), timeValue);
	glUniform1i(glGetUniformLocation(alphaShaderProgram, "showtrianglemesh"), showTriangleMesh ? 1 : 0);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D_ARRAY, _textureManager.getTextureArray());
	glUniform1i(glGetUniformLocation(alphaShaderProgram, "textureArray"), 0);

	glDisable(GL_BLEND);
	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
	glDisable(GL_CULL_FACE);
	_chunkMgr.renderTransparentBlocks();
	_chunkMgr.setViewProj(prevView, projectionMatrix);

	// 2) Add the sun sprite to the planar reflection so it can reflect in water
	{
		glUseProgram(sunShaderProgram);
		// Use full mirrored view (with translation) and oblique projection
		glUniformMatrix4fv(glGetUniformLocation(sunShaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(viewFullMirror));
		glUniformMatrix4fv(glGetUniformLocation(sunShaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projOblique));

		// Place sun far along its world-space direction relative to the mirrored camera
		glm::vec3 sunDir = computeSunDirection(timeValue);
		glm::vec3 sunPos = camMir + sunDir * 6000.0f;
		glUniform3fv(glGetUniformLocation(sunShaderProgram, "sunPosition"), 1, glm::value_ptr(sunPos));

		glm::vec3 sunColor(1.0f, 0.6f, 0.2f);
		float height = clamp((sunPos.y - camMir.y) / 6000.0f, 0.0f, 1.0f);
		sunColor = mix(sunColor, glm::vec3(1.0f), height);
		glUniform3fv(glGetUniformLocation(sunShaderProgram, "sunColor"), 1, glm::value_ptr(sunColor));
		glUniform1f(glGetUniformLocation(sunShaderProgram, "intensity"), 1.0f);

		glEnable(GL_DEPTH_TEST);   // sun should be occluded by mirrored terrain
		glDepthMask(GL_FALSE);     // but not write depth
		glEnable(GL_BLEND);
		glBlendFunc(GL_ONE, GL_ONE); // additive

		glBindVertexArray(sunVAO);
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
		glBindVertexArray(0);

		glDisable(GL_BLEND);
		glDepthMask(GL_TRUE);
	}

	if (cullWasEnabled)
		glEnable(GL_CULL_FACE);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void StoneEngine::prepareRenderPipeline()
{
	// Default to filled geometry; wireframe overlay handled in renderSolidObjects()
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	if (showTriangleMesh)
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	else
		glBindFramebuffer(GL_FRAMEBUFFER, msaaFBO.fbo);

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);
	glDepthMask(GL_TRUE);
	glMatrixMode(GL_MODELVIEW);
}
void StoneEngine::renderSolidObjects()
{
	activateRenderShader();
	_chunkMgr.updateDrawData();
	_chunkMgr.setViewProj(viewMatrix, projectionMatrix);
	// Provide previous-frame depth to enable conservative occlusion culling
	// Skip when geometry just changed or the camera moved/zoomed a lot
	// to avoid popping and flashes.
	{
		glm::vec3 cam = camera.getWorldPosition();
		glm::vec2 ang = camera.getAngles();
		float     fov = _fov;
		if (_havePrevCam) {
			float posDelta   = glm::length(cam - _prevCamPos);
			float yawDelta   = std::abs(ang.x - _prevCamAngles.x);
			float pitchDelta = std::abs(ang.y - _prevCamAngles.y);
			float rotDelta   = std::max(yawDelta, pitchDelta);
			float fovDelta   = std::abs(fov - _prevFov);
			// Stricter thresholds to stabilize occlusion during close strafing
			// ~0.25m movement, ~1.0 deg rotation, any noticeable FOV change
			if (posDelta > 0.25f || rotDelta > 1.0f || fovDelta > 0.25f) {
				_occlDisableFrames = std::max(_occlDisableFrames, 3);
			}
		}
		_prevCamPos   = cam;
		_prevCamAngles= ang;
		_prevFov      = fov;
		_havePrevCam  = true;
	}
	// Skip occlusion if requested or when in wireframe (triangleMesh) mode
	if (!showTriangleMesh && _occlDisableFrames <= 0 && _prevOccValid) {
		// Use previous-frame view/proj/cam that match readFBO.depth
		_chunkMgr.setOcclusionSource(
			readFBO.depth, windowWidth, windowHeight,
			_prevViewOcc, _prevProjOcc,
			_prevCamOcc
		);
	}
	// In wireframe mode: first fill depth, then overlay wireframe using same culling
	if (showTriangleMesh)
	{
		// Depth pre-pass (fill, depth write on, no color writes)
		glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
		glDepthFunc(GL_LESS);
		glDepthMask(GL_TRUE);
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		(void)_chunkMgr.renderSolidBlocks();

		// Wireframe overlay (line, depth compare against prepass, keep depth)
		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		glDepthFunc(GL_LEQUAL);
		glDepthMask(GL_FALSE);
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		drawnTriangles = _chunkMgr.renderSolidBlocks();

		// Restore default state
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		glDepthFunc(GL_LESS);
		glDepthMask(GL_TRUE);
		glCleanupTextureState();
		return;
	}

	drawnTriangles = _chunkMgr.renderSolidBlocks();
	if (_occlDisableFrames > 0)
		--_occlDisableFrames;
	glCleanupTextureState();
}

void StoneEngine::screenshotFBOBuffer(FBODatas &source, FBODatas &destination)
{
	if (showTriangleMesh)
		return;
	// Copy from current write to current read
	glBindFramebuffer(GL_READ_FRAMEBUFFER, source.fbo);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, destination.fbo);
	glBlitFramebuffer(0, 0, windowWidth, windowHeight,
					  0, 0, windowWidth, windowHeight,
					  GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT, GL_NEAREST);
}

void StoneEngine::swapPingPongBuffers()
{
	if (showTriangleMesh)
		return;

	FBODatas tmp = writeFBO;
	writeFBO = readFBO;
	readFBO = tmp;
}

void StoneEngine::sendPostProcessFBOToDispay(const FBODatas &sourceFBO)
{
	if (showTriangleMesh)
		return;
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, sourceFBO.fbo);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
	glBlitFramebuffer(0, 0, windowWidth, windowHeight,
					  0, 0, windowWidth, windowHeight,
					  GL_COLOR_BUFFER_BIT, GL_NEAREST);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
	glDepthMask(GL_TRUE);
	glClear(GL_DEPTH_BUFFER_BIT);
}

void StoneEngine::renderTransparentObjects()
{
	// 1) Masked alpha (leaves): depth write ON, no blending, no culling
	glDisable(GL_BLEND);
	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
	glDisable(GL_CULL_FACE);
	{
		// Activate alpha shader and draw only leaves (shader filters by TextureID)
		mat4 modelMatrix = mat4(1.0f);
		float radY, radX;
		radX = camera.getAngles().x * (M_PI / 180.0);
		radY = camera.getAngles().y * (M_PI / 180.0);
		mat4 viewRot = mat4(1.0f);
		viewRot = rotate(viewRot, radY, vec3(-1.0f, 0.0f, 0.0f));
		viewRot = rotate(viewRot, radX, vec3(0.0f, -1.0f, 0.0f));
		mat4 viewFull = viewRot;
		viewFull = translate(viewFull, vec3(camera.getPosition()));
		this->viewMatrix = viewFull;
		_chunkMgr.setViewProj(this->viewMatrix, projectionMatrix);
		vec3 viewPos = camera.getWorldPosition();
		glUseProgram(alphaShaderProgram);
		glUniformMatrix4fv(glGetUniformLocation(alphaShaderProgram, "projection"), 1, GL_FALSE, value_ptr(projectionMatrix));
		glUniformMatrix4fv(glGetUniformLocation(alphaShaderProgram, "model"), 1, GL_FALSE, value_ptr(modelMatrix));
		glUniformMatrix4fv(glGetUniformLocation(alphaShaderProgram, "view"), 1, GL_FALSE, value_ptr(viewRot));
		glUniform3fv(glGetUniformLocation(alphaShaderProgram, "cameraPos"), 1, value_ptr(viewPos));
		glUniform3fv(glGetUniformLocation(alphaShaderProgram, "lightColor"), 1, value_ptr(vec3(1.0f, 0.95f, 0.95f)));
		glUniform1f(glGetUniformLocation(alphaShaderProgram, "time"), (float)glfwGetTime());
		glUniform1i(glGetUniformLocation(alphaShaderProgram, "timeValue"), timeValue);
		// Ensure leaves render in wireframe when toggled
		glUniform1i(glGetUniformLocation(alphaShaderProgram, "showtrianglemesh"), showTriangleMesh ? 1 : 0);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D_ARRAY, _textureManager.getTextureArray());
		glUniform1i(glGetUniformLocation(alphaShaderProgram, "textureArray"), 0);
		// Use already-uploaded transparent buffers this frame; do not update here
		if (showTriangleMesh) {
			// Draw leaf faces as wireframe lines
			glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		}
		_chunkMgr.renderTransparentBlocks();
	}

	// --- Only do MSAA bookkeeping when NOT in wireframe ---
	if (!showTriangleMesh)
	{
		// Update SSR source to include leaves before drawing water
		resolveMsaaToFbo(writeFBO, /*copyDepth=*/true);
		blitColorDepth(writeFBO, readFBO);

		// Restore draw target to MSAA FBO (resolve changed GL_DRAW_FRAMEBUFFER)
		glBindFramebuffer(GL_FRAMEBUFFER, msaaFBO.fbo);
	}
	else
	{
		// Wireframe: keep drawing to the default framebuffer
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	// 2) Water
	activateTransparentShader();         // already sets wireframe-friendly state when showTriangleMesh==true
	drawnTriangles += _chunkMgr.renderTransparentBlocks();

	// Restore polygon mode after wireframe water rendering
	if (showTriangleMesh)
	{
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	}

	glCleanupTextureState();
}

void StoneEngine::renderSkybox()
{
	if (showTriangleMesh)
		return; // keep wireframe clean
	if (!_hasSkybox || skyboxProgram == 0)
	{
		static bool once = false;
		if (!once)
		{
			std::cerr << "[Skybox] Skip render: has=" << _hasSkybox << " prog=" << skyboxProgram << std::endl;
			once = true;
		}
		return;
	}

	// Ensure drawing targets the current MSAA buffer
	glBindFramebuffer(GL_FRAMEBUFFER, msaaFBO.fbo);

	// Rotation-only view so the cube follows the camera
	float radY = camera.getAngles().y * (M_PI / 180.0f);
	float radX = camera.getAngles().x * (M_PI / 180.0f);
	glm::mat4 viewRot(1.0f);
	viewRot = glm::rotate(viewRot, radY, glm::vec3(-1.0f, 0.0f, 0.0f));
	viewRot = glm::rotate(viewRot, radX, glm::vec3(0.0f, -1.0f, 0.0f));

	glUseProgram(skyboxProgram);
	// Debug: verify texture/program once
	// static bool infoOnce=false;
	// if(!infoOnce){ std::cerr << "[Skybox] Render with program=" << skyboxProgram << " tex=" << _skybox.getTextureID() << std::endl; infoOnce=true; }
	glUniformMatrix4fv(glGetUniformLocation(skyboxProgram, "view"), 1, GL_FALSE, glm::value_ptr(viewRot));
	glUniformMatrix4fv(glGetUniformLocation(skyboxProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projectionMatrix));

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_CUBE_MAP, _skybox.getTextureID());
	glUniform1i(glGetUniformLocation(skyboxProgram, "skybox"), 0);

	// Render inside of the cube. Disable face culling to avoid orientation issues.
	GLboolean cullEnabled = glIsEnabled(GL_CULL_FACE);
	glDisable(GL_CULL_FACE);

	_skybox.render();

	if (cullEnabled)
		glEnable(GL_CULL_FACE);
	glCleanupTextureState();
}

void StoneEngine::renderOverlayAndUI()
{
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glUseProgram(0);

	// Refresh UI-visible counters from worker-thread atomics
	_chunkMgr.snapshotDebugCounters();

	glDisable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_STENCIL_TEST);
	glDisable(GL_SCISSOR_TEST);

	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL); // <— ensure text isn’t wireframe

	glViewport(0, 0, windowWidth, windowHeight);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glActiveTexture(GL_TEXTURE0); // <— make unit 0 active for fixed pipeline text

	if (showHelp)
	{
		updateHelpStatusText();
		helpBox.render();
	}
	else if (showDebugInfo)
	{
		debugBox.render();
	}
}

void StoneEngine::finalizeFrame()
{
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	glDepthMask(GL_TRUE);
	glDisable(GL_BLEND);
	calculateFps();
	glfwSwapBuffers(_window);
}

void StoneEngine::loadFirstChunks()
{
	ivec2 chunkPos = camera.getChunkPosition(CHUNK_SIZE);
	chronoHelper.startChrono(0, "Load chunks");
	_chunkMgr.loadChunks(chunkPos);
	chronoHelper.stopChrono(0);
	chronoHelper.printChronos();
}

void StoneEngine::loadNextChunks(ivec2 newCamChunk)
{
	chronoHelper.startChrono(0, "Load chunks");

	// Safer sequencing at low render distances:
	// 1) Load new chunks for the new camera center
	// 2) Then unload chunks outside the radius
	// This avoids transient empty displayed sets while a build is occurring.
	if (getIsRunning())
	{
		std::future<void> loadRet = _pool.enqueue(&ChunkManager::loadChunks, &_chunkMgr, newCamChunk);
		while (loadRet.wait_for(std::chrono::milliseconds(10)) == std::future_status::timeout)
		{
			if (!getIsRunning()) break;
		}
		if (loadRet.valid()) loadRet.get();
	}

	if (getIsRunning())
	{
		std::future<void> unloadRet = _pool.enqueue(&ChunkManager::unloadChunks, &_chunkMgr, newCamChunk);
		while (unloadRet.wait_for(std::chrono::milliseconds(10)) == std::future_status::timeout)
		{
			if (!getIsRunning()) break;
		}
		if (unloadRet.valid()) unloadRet.get();
	}

	chronoHelper.stopChrono(0);
	chronoHelper.printChrono(0);
}

ivec2 StoneEngine::getChunkPos(vec2 camPosXZ)
{
	// Convert camera-space coords to world-space and use floor division
	const float wx = -camPosXZ.x;
	const float wz = -camPosXZ.y;
	const float cs = static_cast<float>(CHUNK_SIZE);
	return {
		static_cast<int>(std::floor(wx / cs)),
		static_cast<int>(std::floor(wz / cs))};
}

void StoneEngine::updateMovement()
{
	vec3 oldPos = camera.getWorldPosition();
	_player.updateMovement();
	vec3 newPos = camera.getWorldPosition();
	if (newPos != oldPos)
	{
		glUseProgram(shaderProgram);
		// legacy: viewPos may not be used; keep harmless
		glUniform3fv(glGetUniformLocation(shaderProgram, "viewPos"), 1, value_ptr(newPos));
		glUniform3fv(glGetUniformLocation(shaderProgram, "cameraPos"), 1, value_ptr(newPos));
	}
}

void StoneEngine::updateGameTick()
{
	if (!pauseTime)
		timeValue += 6; // Increment time value per game tick
	// std::cout << "timeValue: " << timeValue << std::endl;
	// _chunkMgr.printSizes(); Debug container sizes
	if (timeValue > 86400)
		timeValue = 0;
	if (timeValue < 0)
		timeValue = 86400;
	if (showLight)
	{
		glUseProgram(shaderProgram);
		glUniform1i(glGetUniformLocation(shaderProgram, "timeValue"), timeValue);
		glUseProgram(postProcessShaders[GREEDYFIX].program);
		glUniform1i(glGetUniformLocation(postProcessShaders[GREEDYFIX].program, "timeValue"), timeValue);
	}
	else
	{
		// Always daytime
		glUseProgram(shaderProgram);
		glUniform1i(glGetUniformLocation(shaderProgram, "timeValue"), 52000);
		glUseProgram(postProcessShaders[GREEDYFIX].program);
		glUniform1i(glGetUniformLocation(postProcessShaders[GREEDYFIX].program, "timeValue"), 52000);
	}
	_player.updateSwimSpeed();
}

void StoneEngine::updateChunkWorker()
{
	bool firstIteration = true;
	ivec2 oldCamChunk = camera.getChunkPosition(CHUNK_SIZE);
	vec3 oldCamPos = camera.getPosition();

	while (getIsRunning())
	{
		vec3 newCamPos = camera.getPosition();
		if (oldCamPos.x != newCamPos.x || oldCamPos.z != newCamPos.z || firstIteration)
		{
			// Check new chunk position for necessary updates to chunks
			ivec2 camChunk = camera.getChunkPosition(CHUNK_SIZE);
			if (firstIteration)
			{
				loadFirstChunks();
				firstIteration = false;
			}
			else if (updateChunk && (floor(oldCamChunk.x) != floor(camChunk.x) || floor(oldCamChunk.y) != floor(camChunk.y)))
			{
				loadNextChunks(camChunk);
			}
			oldCamPos = newCamPos;
			oldCamChunk = camChunk;
		}
	}
}

void StoneEngine::updateBiomeData()
{
	vec3 viewPos = camera.getWorldPosition();
	ivec2 blockPos = {int(viewPos.x), int(viewPos.z)};
	double height = noise_gen.getHeight(blockPos);
	_biome = int(noise_gen.getBiome(blockPos, height));

	// Latitudinal bands (very low frequency): use trigs to create belts
	double alt01 = std::clamp((height - (double)OCEAN_HEIGHT) / (double)(MOUNT_HEIGHT - OCEAN_HEIGHT), 0.0, 1.0);

	double latS = std::sin(blockPos.x * 0.00003);
	double latC = std::cos(blockPos.y * 0.000008);

	// Continentalness: interiors tend to be drier
	double continental = noise_gen.getContinentalNoise(blockPos); // [-1, 1]
	double tempBias = (latS * 0.6) + (latC * 0.2) - (alt01 * 0.8);
	double humidBias = (-continental * 0.5)						 // wetter near coasts (low continentalness)
					   + ((1.0 - alt01) * 0.2)					 // more humidity at low elevations
					   + (std::cos(blockPos.y * 0.00002) * 0.3); // tropical/rain belts

	// Base noises in [-1, 1]
	double temp = noise_gen.getTemperatureNoise(blockPos);
	double humidity = noise_gen.getHumidityNoise(blockPos);
	temp = std::clamp(temp + tempBias, -1.0, 1.0);
	humidity = std::clamp(humidity + humidBias, -1.0, 1.0);
	_humidity = humidity;
	_temperature = temp;
}

void StoneEngine::update()
{
	// Time acceleration (numeric keypad + / -)
	_timeAccelerating = (accelPlus || accelMinus);
	if (accelPlus)  { timeValue += 50; }
	if (accelMinus) { timeValue -= 50; }

	_player.findMoveRotationSpeed();
	// Get current time
	end = std::chrono::steady_clock::now();
	now = end;
	delta = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
	start = end; // Reset start time for next frame

	// Check for delta and apply to move and rotation speeds
	_player.updateNow(now);

	// Fixed 20 Hz world tick (day/night, water nudges), independent of FPS
	static auto tickPrev = std::chrono::steady_clock::now();
	static double tickAcc = 0.0;
	const double tickStep = 1.0 / 20.0;
	tickAcc += std::chrono::duration<double>(end - tickPrev).count();
	tickPrev = end;
	int safety = 0;
	while (tickAcc >= tickStep && safety < 20)
	{
		updateGameTick();
		tickAcc -= tickStep;
		++safety;
	}

	// Update player states
	_player.updatePlayerStates();

	// Update player data
	if (_player.updatePlacing())
		_occlDisableFrames = std::max(_occlDisableFrames, 2);
	_player.updatePlayerDirection();
	_player.updateMovement();
	updateBiomeData();
	display();
}

void StoneEngine::resetFrameBuffers()
{
	// Read/Write/Tmp framebuffers memory free
	glDeleteFramebuffers(1, &readFBO.fbo);
	glDeleteTextures(1, &readFBO.texture);
	glDeleteTextures(1, &readFBO.depth);
	glDeleteFramebuffers(1, &tmpFBO.fbo);
	glDeleteTextures(1, &tmpFBO.texture);
	glDeleteTextures(1, &tmpFBO.depth);
	glDeleteFramebuffers(1, &writeFBO.fbo);
	glDeleteTextures(1, &writeFBO.texture);
	glDeleteTextures(1, &writeFBO.depth);

	// Msaa renderbuffers and framebuffer memory free
	glDeleteFramebuffers(1, &msaaFBO.fbo);
	glDeleteRenderbuffers(1, &msaaFBO.texture);
	glDeleteRenderbuffers(1, &msaaFBO.depth);

	// Init second time
	initFramebuffers(readFBO, windowWidth, windowHeight);
	initFramebuffers(tmpFBO, windowWidth, windowHeight);
	initFramebuffers(writeFBO, windowWidth, windowHeight);
	initMsaaFramebuffers(msaaFBO, windowWidth, windowHeight);
}

void StoneEngine::mouseButtonAction(int button, int action, int mods)
{
	(void)mods;
	// Only when mouse is captured (so clicks aren't for UI)
	if (!mouseCaptureToggle)
		return;
	BlockType selectedBlock = _player.getSelectedBlock();

	if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
	{
		// Ray origin and direction in WORLD space
		glm::vec3 origin = camera.getWorldPosition();
		glm::vec3 dir = camera.getDirection();

		// Pre-fetch the block about to be deleted
		glm::ivec3 peek;
		BlockType toDelete = _chunkMgr.raycastHitFetch(origin, dir, 5.0f, peek);
		// If deleting dirt/grass/sand, prefetch above cell type for flower instance cleanup
		glm::ivec3 abovePeek = {peek.x, peek.y + 1, peek.z};
		BlockType aboveBefore = AIR;
		if (toDelete == DIRT || toDelete == GRASS || toDelete == SAND)
		{
			glm::ivec2 aboveChunkPos(
				(int)std::floor((float)abovePeek.x / (float)CHUNK_SIZE),
				(int)std::floor((float)abovePeek.z / (float)CHUNK_SIZE));
			aboveBefore = _chunkMgr.getBlock(aboveChunkPos, abovePeek);
		}
		// Delete the first solid block within 5 blocks of reach
		bool deleted = _chunkMgr.raycastDeleteOne(origin, dir, 5.0f);
		if (deleted)
		{
			// Disable occlusion briefly to prevent one-frame pop after edit
			_occlDisableFrames = std::max(_occlDisableFrames, 2);
			if (toDelete == FLOWER_POPPY || toDelete == FLOWER_DANDELION || toDelete == FLOWER_CYAN || toDelete == FLOWER_SHORT_GRASS || toDelete == FLOWER_DEAD_BUSH)
			{
				removeFlowerAtCell(peek);
			}
			// If the broken block is dirt/grass/sand and there was a flower above, remove its instance as well
			if ((toDelete == DIRT || toDelete == GRASS || toDelete == SAND) &&
				(aboveBefore == FLOWER_POPPY || aboveBefore == FLOWER_DANDELION ||
				 aboveBefore == FLOWER_CYAN  || aboveBefore == FLOWER_SHORT_GRASS ||
				 aboveBefore == FLOWER_DEAD_BUSH))
			{
				removeFlowerAtCell(abovePeek);
			}
		}
	}
	else if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS && _player.getSelectedBlock() != AIR)
	{
		// Ray origin and direction in WORLD space
		glm::vec3 origin = camera.getWorldPosition();
		glm::vec3 dir = camera.getDirection();

		// Place block 5 block range
		glm::ivec3 placedAt{0};
		bool placed = _chunkMgr.raycastPlaceOne(origin, dir, 5.0f, selectedBlock, placedAt);
		if (placed)
		{
			_occlDisableFrames = std::max(_occlDisableFrames, 2);
			// If a non-flower block was placed, ensure any existing flower instance at that cell is removed
			if (!(selectedBlock == FLOWER_POPPY || selectedBlock == FLOWER_DANDELION ||
				  selectedBlock == FLOWER_CYAN  || selectedBlock == FLOWER_SHORT_GRASS))
			{
				removeFlowerAtCell(placedAt);
			}
			// If placing a flower, spawn a render instance too
			int typeId = -1;
			if (selectedBlock == FLOWER_POPPY)
				typeId = (_layerPoppy >= 0) ? _layerPoppy : 0;
			else if (selectedBlock == FLOWER_DANDELION)
				typeId = (_layerDandelion >= 0) ? _layerDandelion : 0;
			else if (selectedBlock == FLOWER_CYAN)
				typeId = (_layerCyan >= 0) ? _layerCyan : 0;
			else if (selectedBlock == FLOWER_SHORT_GRASS)
				typeId = (flowerShortGrassLayer >= 0) ? flowerShortGrassLayer : 0;
			else if (selectedBlock == FLOWER_DEAD_BUSH)
				typeId = (_layerDeadBush >= 0) ? _layerDeadBush : 0;
			if (typeId >= 0)
			{
				static std::mt19937 rng{std::random_device{}()};
				std::uniform_real_distribution<float> rotD(-0.26f, 0.26f);
				std::uniform_real_distribution<float> scaD(0.95f, 1.05f);
				glm::vec3 center = glm::vec3(placedAt.x + 0.5f, placedAt.y + 0.0f, placedAt.z + 0.5f);
				if (selectedBlock == FLOWER_POPPY)
					center.y -= 0.05f;
				addFlower(center, typeId, rotD(rng), scaD(rng), 1.0f);
			}
		}
		_player.setPlacing(true);

		// Start cooldown immediately to avoid a second place on the same frame
		_player.setPlaceCd();
	}
	else if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_RELEASE && selectedBlock != AIR)
		_player.setPlacing(false);
	else if (button == GLFW_MOUSE_BUTTON_MIDDLE && action == GLFW_PRESS)
	{
		// Ray origin and direction in WORLD space
		glm::vec3 origin = camera.getWorldPosition();
		glm::vec3 dir = camera.getDirection();
		glm::ivec3 hit;
		BlockType blockFound;

		blockFound = _chunkMgr.raycastHitFetch(origin, dir, 5.0f, hit);

		// Guard from selecting any type of blocks
		if (blockFound != BEDROCK && blockFound != AIR)
		{
			selectedBlock = blockFound;

			// For debug textbox
			for (size_t i = 0; i < NB_BLOCKS; i++)
			{
				if (blockDebugTab[i].correspondance == blockFound)
				{
					selectedBlockDebug = blockDebugTab[i].block;
					_player.setSelectedBlock(blockFound);
					return;
				}
			}
		}
	}
}

void StoneEngine::mouseButtonCallback(GLFWwindow *window, int button, int action, int mods)
{
	StoneEngine *engine = static_cast<StoneEngine *>(glfwGetWindowUserPointer(window));
	if (engine)
		engine->mouseButtonAction(button, action, mods);
}

void StoneEngine::reshapeAction(int width, int height)
{
	glViewport(0, 0, width, height);
	glMatrixMode(GL_PROJECTION);

	windowHeight = height;
	windowWidth = width;
	if (!_isFullscreen)
	{
		_windowedW = width;
		_windowedH = height;
		int px, py;
		glfwGetWindowPos(_window, &px, &py);
		_windowedX = px;
		_windowedY = py;
	}
	resetFrameBuffers();
	// On actual window resize, previous-frame depth is invalid for occlusion
	_occlDisableFrames = std::max(_occlDisableFrames, 3);
	_prevOccValid = false;
	float y = NEAR_PLANE;
	projectionMatrix = perspective(radians(_fov), float(width) / float(height), y, FAR_PLANE);
	glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, value_ptr(projectionMatrix));

	// Update post-process shaders with new texel size so screen-space effects (e.g., crosshair)
	// remain correctly centered after a resize.
	for (auto &entry : postProcessShaders)
	{
		updateFboWindowSize(entry.second);
	}
}

void StoneEngine::reshape(GLFWwindow *window, int width, int height)
{
	StoneEngine *engine = static_cast<StoneEngine *>(glfwGetWindowUserPointer(window));

	if (engine)
		engine->reshapeAction(width, height);
}

void StoneEngine::keyAction(int key, int scancode, int action, int mods)
{
	(void)scancode;
	(void)mods;
	if (action == GLFW_PRESS && key == GLFW_KEY_F)
	{
		// Change FOV without rebuilding framebuffers to avoid culling flashes
		_fov = 80.0f;
		// Update projection only; viewport is unchanged
		projectionMatrix = perspective(radians(_fov), float(windowWidth) / float(windowHeight), NEAR_PLANE, FAR_PLANE);
	}
	if (action == GLFW_PRESS && key == GLFW_KEY_C)
		updateChunk = !updateChunk;
	if (action == GLFW_PRESS && key == GLFW_KEY_G)
	{
		gravity = !gravity;
		_player.toggleGravity();
	}
	if (action == GLFW_PRESS && key == GLFW_KEY_F4)
		showTriangleMesh = !showTriangleMesh;
	if (action == GLFW_PRESS && key == GLFW_KEY_F11)
	{
		setFullscreen(!_isFullscreen);
	}
	if (action == GLFW_PRESS && key == GLFW_KEY_F1)
		showUI = !showUI;
	if (action == GLFW_PRESS && key == GLFW_KEY_L)
		showLight = !showLight;
	if (action == GLFW_PRESS && key == GLFW_KEY_F3)
	{
		showDebugInfo = !showDebugInfo;
		if (showDebugInfo)
			showHelp = false; // debug replaces help
	}
	if (action == GLFW_PRESS && key == GLFW_KEY_H)
	{
		showHelp = !showHelp;
		if (showHelp)
			showDebugInfo = false; // help replaces debug
	}
	if (action == GLFW_PRESS && (key == GLFW_KEY_M || key == GLFW_KEY_SEMICOLON))
		mouseCaptureToggle = !mouseCaptureToggle;
	if (action == GLFW_PRESS && (key == GLFW_KEY_F5)) camera.invert();
	if (action == GLFW_PRESS && (key == GLFW_KEY_P)) pauseTime = !pauseTime;
	if (key == GLFW_KEY_ESCAPE) glfwSetWindowShouldClose(_window, GL_TRUE);
	if (action == GLFW_PRESS && key == GLFW_KEY_F6) {
		_gridMode = static_cast<GridDebugMode>((int(_gridMode) + 1) % 4);
	}
	if (action == GLFW_PRESS && key == GLFW_KEY_LEFT_CONTROL)
		_player.toggleSprint();
	if (action == GLFW_PRESS && key == GLFW_KEY_KP_ADD)
		accelPlus = true;
	else if (action == GLFW_RELEASE && key == GLFW_KEY_KP_ADD)
		accelPlus = false;
	if (action == GLFW_PRESS && key == GLFW_KEY_KP_SUBTRACT)
		accelMinus = true;
	if (action == GLFW_RELEASE && key == GLFW_KEY_KP_SUBTRACT)
		accelMinus = false;
	_player.keyAction(key, scancode, action, mods);
}

void StoneEngine::keyPress(GLFWwindow *window, int key, int scancode, int action, int mods)
{
	StoneEngine *engine = static_cast<StoneEngine *>(glfwGetWindowUserPointer(window));

	if (engine)
		engine->keyAction(key, scancode, action, mods);
}

void StoneEngine::mouseAction(double x, double y)
{
	if (!mouseCaptureToggle)
	{
		glfwSetInputMode(_window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
		return;
	}
	static bool firstMouse = true;
	static double lastX = 0, lastY = 0;

	// Disable cursor
	glfwSetInputMode(_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

	camera.updateMousePos(x, y);

	if (firstMouse)
	{
		lastX = x;
		lastY = y;
		firstMouse = false;
		return;
	}

	float xOffset = static_cast<float>(lastX - x);
	float yOffset = static_cast<float>(lastY - y);

	lastX = x;
	lastY = y;

	float sensitivity = 0.05f;
	xOffset *= sensitivity;
	yOffset *= sensitivity;

	camera.rotate(1.0f, 0.0f, xOffset * ROTATION_SPEED);
	camera.rotate(0.0f, 1.0f, yOffset * ROTATION_SPEED);
}

void StoneEngine::mouseCallback(GLFWwindow *window, double x, double y)
{
	StoneEngine *engine = static_cast<StoneEngine *>(glfwGetWindowUserPointer(window));

	if (engine)
		engine->mouseAction(x, y);
}

void StoneEngine::scrollAction(double yoffset)
{
	_fov -= (float)yoffset;
	_fov = std::clamp(_fov, 1.0f, 90.0f);

	// Update projection only; avoid full framebuffer reset to prevent flashes
	projectionMatrix = perspective(radians(_fov), float(windowWidth) / float(windowHeight), NEAR_PLANE, FAR_PLANE);
}

void StoneEngine::scrollCallback(GLFWwindow *window, double xoffset, double yoffset)
{
	(void)xoffset;
	StoneEngine *engine = static_cast<StoneEngine *>(glfwGetWindowUserPointer(window));

	if (engine)
		engine->scrollAction(yoffset);
}

int StoneEngine::initGLFW()
{
	glfwWindowHint(GLFW_DEPTH_BITS, 32); // Request 32-bit depth buffer
	// glfwWindowHint(GLFW_SAMPLES, 4);

	// Always start in true fullscreen on the primary monitor
	GLFWmonitor *monitor = glfwGetPrimaryMonitor();
	const GLFWvidmode *mode = monitor ? glfwGetVideoMode(monitor) : nullptr;
	if (mode)
	{
		windowWidth = mode->width;
		windowHeight = mode->height;
		// Match the monitor's color depth and refresh rate for smooth fullscreen
		glfwWindowHint(GLFW_RED_BITS, mode->redBits);
		glfwWindowHint(GLFW_GREEN_BITS, mode->greenBits);
		glfwWindowHint(GLFW_BLUE_BITS, mode->blueBits);
		glfwWindowHint(GLFW_REFRESH_RATE, mode->refreshRate);
		_window = glfwCreateWindow(windowWidth, windowHeight, "Not_ft_minecraft | FPS: 0", monitor, NULL);
	}
	else
	{
		// Fallback to windowed if monitor/mode not available
		_window = glfwCreateWindow(windowWidth, windowHeight, "Not_ft_minecraft | FPS: 0", NULL, NULL);
	}

	if (!_window)
	{
		std::cerr << "Failed to create GLFW window" << std::endl;
		glfwTerminate();
		return 0;
	}

	glfwSetWindowUserPointer(_window, this);
	glfwSetFramebufferSizeCallback(_window, reshape);
	glfwSetKeyCallback(_window, keyPress);
	glfwSetScrollCallback(_window, scrollCallback);
	glfwSetMouseButtonCallback(_window, mouseButtonCallback);
	glfwMakeContextCurrent(_window);
	// Uncapped FPS (disable vsync)
	// glfwSwapInterval(16);
	// Prefer raw mouse motion if supported for smoother, acceleration-free deltas
	if (glfwRawMouseMotionSupported())
		glfwSetInputMode(_window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
	if (!isWSL()) {
		glfwSetCursorPosCallback(_window, mouseCallback);
	}
	_isFullscreen = (glfwGetWindowMonitor(_window) != nullptr);
	return 1;
}

void StoneEngine::initGLEW()
{
	GLenum err = glewInit();
	if (err != GLEW_OK)
	{
		std::cerr << "GLEW initialization failed: " << glewGetErrorString(err) << std::endl;
		return;
	}
	// Reduce seams when sampling across cube faces, especially with mipmaps
	if (GLEW_ARB_seamless_cube_map || GLEW_VERSION_3_2)
	{
		glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
	}
}

void StoneEngine::setFullscreen(bool enable)
{
	if (enable == _isFullscreen)
		return;

	GLFWmonitor *monitor = glfwGetPrimaryMonitor();
	const GLFWvidmode *mode = monitor ? glfwGetVideoMode(monitor) : nullptr;
	if (enable && monitor && mode)
	{
		// Going fullscreen: remember current windowed placement
		int x, y, w, h;
		glfwGetWindowPos(_window, &x, &y);
		glfwGetWindowSize(_window, &w, &h);
		_windowedX = x;
		_windowedY = y;
		_windowedW = w;
		_windowedH = h;

		glfwSetWindowMonitor(_window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
		_isFullscreen = true;
	}
	else
	{
		// Going windowed: use fixed size
		int w = WINDOWED_FIXED_W;
		int h = WINDOWED_FIXED_H;
		int x = _windowedX, y = _windowedY;
		if (mode && monitor)
		{
			// Center if unknown position
			if (x <= 0 && y <= 0)
			{
				x = (mode->width - w) / 2;
				y = (mode->height - h) / 2;
			}
		}
		// Ensure window decorations and non-maximized state when leaving fullscreen
		glfwSetWindowMonitor(_window, nullptr, x, y, w, h, 0);
		glfwSetWindowAttrib(_window, GLFW_DECORATED, GLFW_TRUE);
		glfwRestoreWindow(_window);
		_isFullscreen = false;
	}
}

bool StoneEngine::getIsRunning()
{
	std::lock_guard<std::mutex> lockGuard(_isRunningMutex);
	return _isRunning;
}
