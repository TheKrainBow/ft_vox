#include "StoneEngine.hpp"

float mapRange(float x, float in_min, float in_max, float out_min, float out_max) {
	return out_max - (x - in_min) * (out_max - out_min) / (in_max - in_min);
}

float mapExpo(float x, float in_min, float in_max, float out_min, float out_max) {
	float t = (x - in_min) / (in_max - in_min); // normalize to [0, 1]
	return out_min * std::pow(out_max / out_min, t); // exponential interpolation
}

bool isTransparent(char block)
{
	return block == AIR || block == WATER || block == LOG;
}

// Display logs only if sides
bool faceDisplayCondition(char blockToDisplay, char neighborBlock, Direction dir)
{
	return ((isTransparent(neighborBlock) && blockToDisplay != neighborBlock) || (blockToDisplay == LOG && (dir <= EAST)));
}

void StoneEngine::updateFboWindowSize(PostProcessShader &shader)
{
	float texelX = 1.0f / windowWidth;
	float texelY = 1.0f / windowHeight;

	GLint texelSizeLoc = glGetUniformLocation(shader.program, "texelSize");
	glUseProgram(shader.program);
	glUniform2f(texelSizeLoc, texelX, texelY);
}

StoneEngine::StoneEngine(int seed, ThreadPool &pool) : camera(), _world(seed, _textureManager, camera, pool), _pool(pool), noise_gen(seed)
{
	initData();
	initGLFW();
	initGLEW();
	initTextures();
	initRenderShaders();
	initDebugTextBox();
	initFboShaders();
	reshapeAction(windowWidth, windowHeight);
	_world.init(RENDER_DISTANCE);
	_world.setRunning(&_isRunningMutex, &_isRunning);
}

StoneEngine::~StoneEngine()
{
	// Ensure World GL resources are freed before destroying the context
	_world.shutdownGL();

	glDeleteProgram(shaderProgram);
	glDeleteProgram(waterShaderProgram);
	glDeleteProgram(sunShaderProgram);

	// Clean up post-process shaders
	for (auto& [type, shader] : postProcessShaders)
	{
		glDeleteProgram(shader.program);
		glDeleteVertexArrays(1, &shader.vao);
		glDeleteBuffers(1, &shader.vbo);
	}
	postProcessShaders.clear();

	// Delete sun resources and water normal map
	if (sunVAO) glDeleteVertexArrays(1, &sunVAO);
	if (sunVBO) glDeleteBuffers(1, &sunVBO);
	if (waterNormalMap) glDeleteTextures(1, &waterNormalMap);

	// Delete tmp and MSAA framebuffers/renderbuffers
	if (tmpFBO.fbo) glDeleteFramebuffers(1, &tmpFBO.fbo);
	if (tmpFBO.texture) glDeleteTextures(1, &tmpFBO.texture);
	if (tmpFBO.depth) glDeleteTextures(1, &tmpFBO.depth);

	if (msaaFBO.fbo) glDeleteFramebuffers(1, &msaaFBO.fbo);
	if (msaaFBO.texture) glDeleteRenderbuffers(1, &msaaFBO.texture);
	if (msaaFBO.depth) glDeleteRenderbuffers(1, &msaaFBO.depth);

	glDeleteTextures(1, &readFBO.texture);
	glDeleteTextures(1, &readFBO.depth);
	glDeleteFramebuffers(1, &readFBO.fbo);
	glDeleteTextures(1, &writeFBO.texture);
	glDeleteTextures(1, &writeFBO.depth);
	glDeleteFramebuffers(1, &writeFBO.fbo);
	// Terminate GLFW
	glfwDestroyWindow(_window);
	glfwTerminate();
}

void StoneEngine::run()
{
	_isRunning = true;

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
	if (chunkThread.joinable()) chunkThread.join();
}

void StoneEngine::initData()
{
	// Keys states and runtime booleans
	bzero(keyStates, sizeof(keyStates));
	ignoreMouseEvent	= IGNORE_MOUSE;
	updateChunk			= ENABLE_WORLD_GENERATION;
	showDebugInfo		= SHOW_DEBUG;
	showTriangleMesh	= SHOW_TRIANGLES;
	mouseCaptureToggle	= CAPTURE_MOUSE;
	showLight			= SHOW_LIGHTING;
	gravity				= GRAVITY;
	falling				= FALLING;
	swimming			= SWIMMING;
	jumping				= JUMPING;
	isUnderWater		= UNDERWATER;
	ascending		= ASCENDING;
	_jumpCooldown		= std::chrono::steady_clock::now();
	
	// Gets the max MSAA (anti aliasing) samples
	_maxSamples = 0;
	glGetIntegerv(GL_MAX_SAMPLES, &_maxSamples);

	if (SCHOOL_SAMPLES)
		_maxSamples = 8;

	// Window size
	windowHeight	= W_HEIGHT;
	windowWidth		= W_WIDTH;

	// FPS counter
	frameCount			= 0;
	lastFrameTime		= 0.0;
	currentFrameTime	= 0.0;
	fps					= 0.0;

	// Debug data
	drawnTriangles	= 0.0;

	// Player data
	moveSpeed		= 0.0;
	rotationSpeed	= 0.0;

	// Game data
	sunPosition = {0.0f, 0.0f, 0.0f};
	timeValue = 39800;
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
		{ T_DIRT, "textures/dirt.ppm" },
		{ T_COBBLE, "textures/cobble.ppm" },
		{ T_STONE, "textures/stone.ppm" },
		{ T_GRASS_SIDE, "textures/grass_block_side.ppm" },
		{ T_GRASS_TOP, "textures/grass_block_top_colored.ppm" },
		{ T_SAND, "textures/sand.ppm" },
		{ T_WATER, "textures/water.ppm" },
		{ T_SNOW, "textures/snow.ppm" },
		{ T_BEDROCK, "textures/bedrock.ppm" },
		{ T_LOG_SIDE, "textures/log_side.ppm" },
		{ T_LOG_TOP, "textures/log_top.ppm" },
		{ T_LEAF, "textures/full_leaves.ppm" },
	});

	glGenTextures(1, &waterNormalMap);
	glBindTexture(GL_TEXTURE_2D, waterNormalMap);

	int width, height, nrChannels;
	unsigned char* data = stbi_load("textures/water_normal.jpg", &width, &height, &nrChannels, 0);

	if (data) {
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0,
					GL_RGB, GL_UNSIGNED_BYTE, data);
		glGenerateMipmap(GL_TEXTURE_2D);
	}
	else {
		std::cerr << "Failed to load water normal map!" << std::endl;
	}
	stbi_image_free(data);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}

glm::vec3 StoneEngine::computeSunPosition(int timeValue, const glm::vec3& cameraPos)
{
	const float pi = 3.14159265f;
	const float radius = 6000.0f;
	const float dayStart = 42000.0f;          // sunrise
	const float dayLen   = 86400.0f - dayStart; // 44400 (day duration)
	const float nightLen = dayStart;           // 42000 (night duration)

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

	return cameraPos + glm::vec3(x, y, z);
}

void initSunQuad(GLuint &vao, GLuint &vbo)
{
	// Quad corners in NDC [-1, 1] (used as offset in clip space)
	float quadVertices[] = {
		-1.0f, -1.0f,
		-1.0f,  1.0f,
			1.0f, -1.0f,
			1.0f,  1.0f
	};

	glGenVertexArrays(1, &vao);
	glGenBuffers(1, &vbo);

	glBindVertexArray(vao);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);

	// Attribute 0 = vec2 aPos
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);

	glBindVertexArray(0);
}

void StoneEngine::initRenderShaders()
{
	shaderProgram = createShaderProgram("shaders/render/terrain.vert", "shaders/render/terrain.frag");
	waterShaderProgram = createShaderProgram("shaders/render/water.vert", "shaders/render/water.frag");
	sunShaderProgram = createShaderProgram("shaders/render/sun.vert", "shaders/render/sun.frag");
	initSunQuad(sunVAO, sunVBO);
}

void StoneEngine::displaySun()
{
	if (showTriangleMesh)
		return ;
	glBindFramebuffer(GL_FRAMEBUFFER, writeFBO.fbo);
	// glDepthFunc(GL_LESS);
	vec3 camPos = camera.getWorldPosition();
	// Compute current sun position based on time
	glm::vec3 sunPos = computeSunPosition(timeValue, camPos);

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

	glEnable(GL_DEPTH_TEST);      // allow occlusion by terrain
	glDepthMask(GL_FALSE);        // prevent writing depth
	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE);  // additive glow
	
	glBindVertexArray(sunVAO);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	glBindVertexArray(0);
	
	glDisable(GL_BLEND);
	glDepthMask(GL_TRUE);
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

StoneEngine::PostProcessShader StoneEngine::createPostProcessShader(PostProcessShader &shader, const std::string& vertPath, const std::string& fragPath)
{
	glGenVertexArrays(1, &shader.vao);
	glGenBuffers(1, &shader.vbo);
	glBindVertexArray(shader.vao);
	glBindBuffer(GL_ARRAY_BUFFER, shader.vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(rectangleVertices), &rectangleVertices, GL_STATIC_DRAW);

	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
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
	// New single-pass light shafts implementation
	createPostProcessShader(postProcessShaders[GODRAYS], "shaders/postProcess/postProcess.vert", "shaders/postProcess/lightshafts.frag");
}

void StoneEngine::initDebugTextBox()
{
	vec3 *camPos = camera.getPositionPtr();
	fvec2 *camAngle = camera.getAnglesPtr();
	e_direction *facing_direction = camera.getDirectionPtr();
	float		*yPos = camera.getYPtr();

	debugBox.initData(_window, 0, 0, 200, 200);
	debugBox.loadFont("textures/CASCADIAMONO.TTF", 20);
	debugBox.addLine("FPS: ", Textbox::DOUBLE, &fps);
	debugBox.addLine("Triangles: ", Textbox::INT, &drawnTriangles);
	debugBox.addLine("Memory Usage: ", Textbox::SIZE_T, _world.getMemorySizePtr());
	debugBox.addLine("RenderDistance: ", Textbox::INT, _world.getRenderDistancePtr());
	debugBox.addLine("CurrentRender: ", Textbox::INT, _world.getCurrentRenderPtr());
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

	// Nice soft sky blue
	glClearColor(0.53f, 0.81f, 0.92f, 1.0f);
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

	// Build rotation-only view for rendering to keep coordinates small in shaders
	mat4 viewRot = mat4(1.0f);
	viewRot = rotate(viewRot, radY, vec3(-1.0f, 0.0f, 0.0f));
	viewRot = rotate(viewRot, radX, vec3(0.0f, -1.0f, 0.0f));

	// Build full view (with translation) for culling and CPU-side use
	mat4 viewFull = viewRot;
	viewFull = translate(viewFull, vec3(camera.getPosition()));

	this->viewMatrix = viewFull;
	_world.setViewProj(this->viewMatrix, projectionMatrix);

	glUseProgram(shaderProgram);
	glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, value_ptr(projectionMatrix));
	glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"),       1, GL_FALSE, value_ptr(modelMatrix));
	glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"),        1, GL_FALSE, value_ptr(viewRot));
	glUniform3fv     (glGetUniformLocation(shaderProgram, "lightColor"),   1, value_ptr(vec3(1.0f, 0.95f, 0.95f)));
	// Pass camera world position explicitly for camera-relative rendering
	vec3 camWorld = camera.getWorldPosition();
	glUniform3fv(glGetUniformLocation(shaderProgram, "cameraPos"), 1, value_ptr(camWorld));

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D_ARRAY, _textureManager.getTextureArray());
	glUniform1i(glGetUniformLocation(shaderProgram, "textureArray"), 0);

	glEnable(GL_CULL_FACE);
	glCullFace(GL_FRONT);
	glFrontFace(GL_CCW);
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
	_world.setViewProj(this->viewMatrix, projectionMatrix);

	vec3 viewPos = camera.getWorldPosition();

	glUseProgram(waterShaderProgram);

	// Matrices & camera
	glUniformMatrix4fv(glGetUniformLocation(waterShaderProgram, "projection"), 1, GL_FALSE, value_ptr(projectionMatrix));
	glUniformMatrix4fv(glGetUniformLocation(waterShaderProgram, "model"),       1, GL_FALSE, value_ptr(modelMatrix));
	glUniformMatrix4fv(glGetUniformLocation(waterShaderProgram, "view"),        1, GL_FALSE, value_ptr(viewRot));
	glUniform3fv     (glGetUniformLocation(waterShaderProgram, "viewPos"),      1, value_ptr(viewPos));
	glUniform3fv     (glGetUniformLocation(waterShaderProgram, "cameraPos"),    1, value_ptr(viewPos));
	glUniform1f      (glGetUniformLocation(waterShaderProgram, "time"),             timeValue);
	glUniform1i      (glGetUniformLocation(waterShaderProgram, "isUnderwater"),     isUnderWater);

	// Provide global water plane height to the water shader (for underwater depth-based effects)
	glUniform1f      (glGetUniformLocation(waterShaderProgram, "waterHeight"),      OCEAN_HEIGHT + 2);

	const float nearPlane = 1;
	const float farPlane  = 9600;
	glUniform1f(glGetUniformLocation(waterShaderProgram, "nearPlane"), nearPlane);
	glUniform1f(glGetUniformLocation(waterShaderProgram, "farPlane"),  farPlane);

	// Tweakables for Beer–Lambert absorption -> alpha
	glUniform1f(glGetUniformLocation(waterShaderProgram, "absorption"), 3.5f);
	glUniform1f(glGetUniformLocation(waterShaderProgram, "minAlpha"),   0.15f);
	glUniform1f(glGetUniformLocation(waterShaderProgram, "maxAlpha"),   0.95f);

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

	// Blending and depth settings for transparent pass
	glDepthMask(GL_FALSE);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_DEPTH_TEST);

	glEnable(GL_CULL_FACE);
	glCullFace(GL_FRONT);
	glFrontFace(GL_CCW);
}

void StoneEngine::resolveMsaaToFbo()
{
	glBindFramebuffer(GL_READ_FRAMEBUFFER, msaaFBO.fbo);    // Source: MSAA
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, writeFBO.fbo);  // Destination: regular FBO

	// Blit COLOR
	glBlitFramebuffer(
		0, 0, windowWidth, windowHeight,
		0, 0, windowWidth, windowHeight,
		GL_COLOR_BUFFER_BIT,
		GL_NEAREST
	);

	// Resolve DEPTH
	glBlitFramebuffer(
		0, 0, windowWidth, windowHeight,
		0, 0, windowWidth, windowHeight,
		GL_DEPTH_BUFFER_BIT,
		GL_NEAREST
	);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void StoneEngine::display() {
	prepareRenderPipeline();

	// The scene is rendered to MSAA framebuffer then resolved to the unsampled write buffer
	renderSolidObjects();
	resolveMsaaToFbo();
	screenshotFBOBuffer(writeFBO, readFBO);

	// postProcessGreedyFix();
	// screenshotFBOBuffer(writeFBO, readFBO);

	// Delay greedy-fix until after transparent pass so it applies to the final scene
	displaySun();
	screenshotFBOBuffer(writeFBO, readFBO);
	
	// Render transparent objects with MSAA (same path as solids), then resolve
	if (!showTriangleMesh) {
		glBindFramebuffer(GL_FRAMEBUFFER, msaaFBO.fbo);
		renderTransparentObjects();
		resolveMsaaToFbo();
	}
	else
	{
		// Wireframe: draw directly to current framebuffer
		renderTransparentObjects();
	}
	screenshotFBOBuffer(writeFBO, readFBO);
	// Now run greedy-fix so it affects the combined scene
	postProcessGreedyFix();
	screenshotFBOBuffer(writeFBO, readFBO);

	postProcessFog();
	
	// Display sun because FOG erased it
	displaySun();
	screenshotFBOBuffer(writeFBO, readFBO);

	// Godrays: single-pass over scene+depth to add shafts
	postProcessGodRays();
	screenshotFBOBuffer(writeFBO, readFBO);

	sendPostProcessFBOToDispay();
	renderOverlayAndUI();
	finalizeFrame();
}

void StoneEngine::postProcessFog()
{
	if (showTriangleMesh)
		return ;
	PostProcessShader& shader = postProcessShaders[FOG];

	glBindFramebuffer(GL_FRAMEBUFFER, writeFBO.fbo);
	glClear(GL_COLOR_BUFFER_BIT);
	glUseProgram(shader.program);
	glBindVertexArray(shader.vao);
	glDisable(GL_DEPTH_TEST);

	// Bind screen texture (color)
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, readFBO.texture);
	glUniform1i(glGetUniformLocation(shader.program, "screenTexture"), 0);

	// Bind depth texture
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, readFBO.depth);  // If shared, keep using dboTexture
	glUniform1i(glGetUniformLocation(shader.program, "depthTexture"), 1);

	// Send render distance for adaptative fog
	GLint loc = glGetUniformLocation(shader.program, "renderDistance");
	// glUniform1i(glGetUniformLocation(shader.program, "isUnderwater"), isUnderWater);

	// Set uniforms (after glUseProgram!)
	glUniform1i(glGetUniformLocation(shader.program, "isUnderwater"), isUnderWater);
	glUniform1f(glGetUniformLocation(shader.program, "waterHeight"), OCEAN_HEIGHT + 2);
	glUniform3fv(glGetUniformLocation(shader.program, "viewPos"), 1, glm::value_ptr(camera.getWorldPosition()));
	const float nearPlane = 1.0f;
	const float farPlane  = 9600.0f;
	glUniform1f(glGetUniformLocation(shader.program, "nearPlane"), nearPlane);
	glUniform1f(glGetUniformLocation(shader.program, "farPlane"),  farPlane);

	auto render = _world.getCurrentRenderPtr();
	if (render) {
		if (*render > _bestRender) {
			_bestRender = *render;
		}
		glUniform1f(loc, _bestRender - 5);
	}
	else
		glUniform1f(loc, RENDER_DISTANCE);

	glClear(GL_COLOR_BUFFER_BIT);
	glDrawArrays(GL_TRIANGLES, 0, 6);
}

void StoneEngine::postProcessGreedyFix()
{
	if (showTriangleMesh)
		return ;

	PostProcessShader& shader = postProcessShaders[GREEDYFIX];

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
	glBindTexture(GL_TEXTURE_2D, readFBO.depth);  // If shared, keep using dboTexture
	glUniform1i(glGetUniformLocation(shader.program, "depthTexture"), 1);

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	glDepthFunc(GL_LESS);
}

void StoneEngine::postProcessGodRays()
{
	if (showTriangleMesh)
		return;

	PostProcessShader& shader = postProcessShaders[GODRAYS];

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
	glm::vec3 sunPos = computeSunPosition(timeValue, camPos);

	// Uniforms
	glUniformMatrix4fv(glGetUniformLocation(shader.program, "view"), 1, GL_FALSE, glm::value_ptr(viewMatrix));
	glUniformMatrix4fv(glGetUniformLocation(shader.program, "projection"), 1, GL_FALSE, glm::value_ptr(projectionMatrix));
	glUniform3fv(glGetUniformLocation(shader.program, "sunPos"), 1, glm::value_ptr(sunPos));
	glUniform1i(glGetUniformLocation(shader.program, "isUnderwater"), isUnderWater);

	// Intensity scales with sun altitude and underwater state
	float sunHeight = glm::clamp((sunPos.y - camPos.y) / 6000.0f, 0.0f, 1.0f);
	float baseIntensity = isUnderWater ? 0.85f : 0.50f;
	float rayIntensity = baseIntensity * sunHeight;
	glUniform1f(glGetUniformLocation(shader.program, "rayIntensity"), rayIntensity);

	glClear(GL_COLOR_BUFFER_BIT);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
}

void StoneEngine::prepareRenderPipeline() {
	if (showTriangleMesh) {
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}
	else {
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		// glBindFramebuffer(GL_FRAMEBUFFER, msaaFBO.fbo);
		glBindFramebuffer(GL_FRAMEBUFFER, msaaFBO.fbo);
	}

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);
	glDepthMask(GL_TRUE);
	glMatrixMode(GL_MODELVIEW);
}

void StoneEngine::renderSolidObjects() {
	activateRenderShader();
	_world.updateDrawData();
	_world.setViewProj(viewMatrix, projectionMatrix);
	drawnTriangles = _world.displaySolid();
}

void StoneEngine::screenshotFBOBuffer(FBODatas &source, FBODatas &destination) {
	if (showTriangleMesh)
		return ;
	// Copy from current write to current read
	glBindFramebuffer(GL_READ_FRAMEBUFFER, source.fbo);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, destination.fbo);
	glBlitFramebuffer(0, 0, windowWidth, windowHeight,
					  0, 0, windowWidth, windowHeight,
					  GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT, GL_NEAREST);
}

void StoneEngine::sendPostProcessFBOToDispay() {
	if (showTriangleMesh)
		return ;
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, writeFBO.fbo);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
	glBlitFramebuffer(0, 0, windowWidth, windowHeight,
						0, 0, windowWidth, windowHeight,
						GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT, GL_NEAREST);
}

void StoneEngine::renderTransparentObjects() {
	activateTransparentShader();
	drawnTriangles += _world.displayTransparent();
}

void StoneEngine::renderOverlayAndUI() {
	activateRenderShader();  // For UI rendering
	glDisable(GL_CULL_FACE);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	if (showDebugInfo)
		debugBox.render();
}

void StoneEngine::finalizeFrame() {
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
	_world.loadFirstChunks(chunkPos);
	chronoHelper.stopChrono(0);
	chronoHelper.printChronos();
}

void StoneEngine::loadNextChunks(ivec2 newCamChunk)
{
	std::future<void> unloadRet;
	std::future<void> loadRet;
	chronoHelper.startChrono(0, "Load chunks");
	if (getIsRunning())
		unloadRet = _pool.enqueue(&World::unLoadNextChunks, &_world, newCamChunk);
	if (getIsRunning())
		loadRet = _pool.enqueue(&World::loadFirstChunks, &_world, newCamChunk);
	unloadRet.get();
	loadRet.get();
	chronoHelper.stopChrono(0);
	chronoHelper.printChrono(0);
}

void StoneEngine::findMoveRotationSpeed()
{
	// Calculate delta time
	static auto lastTime = std::chrono::steady_clock::now();
	auto currentTime = std::chrono::steady_clock::now();
	std::chrono::duration<float> elapsedTime = currentTime - lastTime;
	deltaTime = std::min(elapsedTime.count(), 0.1f);
	lastTime = currentTime;


	// Apply delta to rotation and movespeed
	if (keyStates[GLFW_KEY_LEFT_CONTROL])
		moveSpeed = (MOVEMENT_SPEED * ((20.0 * !gravity) + (2 * gravity))) * deltaTime;
	else
		moveSpeed = MOVEMENT_SPEED * deltaTime;
	
	if (swimming)
		moveSpeed *= 0.5;

	//ZOOM
	if (keyStates[GLFW_KEY_KP_ADD]) {timeValue+=50;}
	if (keyStates[GLFW_KEY_KP_SUBTRACT]) {timeValue-=50;}

	if (!isWSL())
		rotationSpeed = (ROTATION_SPEED - 1.5) * deltaTime;
	else
		rotationSpeed = ROTATION_SPEED * deltaTime;
	start = std::chrono::steady_clock::now();
}

ivec2 StoneEngine::getChunkPos(vec2 camPosXZ)
{
	// Convert camera-space coords to world-space and use floor division
	const float wx = -camPosXZ.x;
	const float wz = -camPosXZ.y;
	const float cs = static_cast<float>(CHUNK_SIZE);
	return {
		static_cast<int>(std::floor(wx / cs)),
		static_cast<int>(std::floor(wz / cs))
	};
}

void StoneEngine::updateFalling(vec3 &worldPos, int &blockHeight)
{
	// Snap landing to target eye height
	const float eyeTarget = blockHeight + 1 + EYE_HEIGHT;

	if (!falling && worldPos.y > eyeTarget + EPS) falling = true;
	if (falling && worldPos.y <= eyeTarget + EPS)
	{
		falling = false;
		fallSpeed = 0.0f;
		camera.setPos({-worldPos.x, -eyeTarget, -worldPos.z});
	}
	camera.move({0.0f, -(fallSpeed * deltaTime), 0.0f});
}

void StoneEngine::updateSwimming(BlockType block)
{
	if (!swimming && block == WATER)
	{
		swimming = true;
	}
	if (swimming && block != WATER)
	{
		swimming = false;
		fallSpeed = 0.0;
		_swimUpCooldownOnRise = std::chrono::steady_clock::now() + std::chrono::milliseconds(250);
	}
}

void StoneEngine::updateJumping()
{
	jumping = gravity && !falling && keyStates[GLFW_KEY_SPACE] && now > _jumpCooldown && !swimming;
}

void StoneEngine::updatePlayerStates()
{
	vec3 worldPos = camera.getWorldPosition();
	BlockType camStandingBlock = AIR;
	BlockType camBodyBlockLegs = AIR;
	BlockType camBodyBlockTorso = AIR;

	// Compute world integer cell first
	int worldX = static_cast<int>(std::floor(worldPos.x));
	int worldZ = static_cast<int>(std::floor(worldPos.z));

	// Derive chunk strictly from the integer world cell to avoid float-boundary mismatches
	ivec2 chunkPos = {
		static_cast<int>(std::floor(static_cast<float>(worldX) / static_cast<float>(CHUNK_SIZE))),
		static_cast<int>(std::floor(static_cast<float>(worldZ) / static_cast<float>(CHUNK_SIZE)))
	};

	// Compute foot cell from eye height
	int footCell = static_cast<int>(std::floor(worldPos.y - EYE_HEIGHT + EPS));
	// int footSubY = static_cast<int>(std::floor(static_cast<float>(footCell) / static_cast<float>(CHUNK_SIZE)));

	// If the destination chunk or the needed subchunk is not yet loaded, avoid updating
	// ground/physics to prevent erroneous falling while streaming catches up.
	// if (Chunk* c = _world.getChunk(chunkPos); c == nullptr || c->getSubChunk(std::max(0, footSubY)) == nullptr)
	// 	return;

	camTopBlock = _world.findBlockUnderPlayer(chunkPos, {worldX, static_cast<int>(std::floor(worldPos.y)), worldZ});
	camStandingBlock   = _world.getBlock(chunkPos, {worldX, footCell - 1, worldZ});
	camBodyBlockLegs   = _world.getBlock(chunkPos, {worldX, footCell,     worldZ});
	camBodyBlockTorso  = _world.getBlock(chunkPos, {worldX, footCell + 1, worldZ});

	// Consider underwater slightly sooner by biasing eye sample downward
	const float eyeBias = 0.10f;
	int eyeCellY = static_cast<int>(std::floor(worldPos.y - eyeBias));
	BlockType camHeadBlock = _world.getBlock(chunkPos, {worldX, eyeCellY, worldZ});
	isUnderWater = (camHeadBlock == WATER);

	// Body blocks states debug
	// std::cout << '[' << camStandingBlock << ']' << std::endl;
	// std::cout << '[' << camBodyBlockLegs << ']' << std::endl;
	// std::cout << '[' << camBodyBlockTorso << ']' << std::endl;

	BlockType inWater = (camStandingBlock == WATER
		|| camBodyBlockLegs == WATER
		|| camBodyBlockTorso == WATER) ? WATER : AIR;
		
	BlockType camBodyOverHead = AIR;
	ascending = fallSpeed > 0.0;
	if (ascending)
	{
		// Ceiling check
		camBodyOverHead = _world.getBlock(chunkPos, {worldX, footCell + 2, worldZ});
		if (camBodyOverHead != WATER && camBodyOverHead != AIR)
		{
			falling = false;
			fallSpeed = 0.0;
		}
	}
	updateFalling(worldPos, camTopBlock.height);
	updateSwimming(inWater);
	updateJumping();
}

bool StoneEngine::canMove(const glm::vec3& offset, float extra)
{
	glm::vec3 probe = offset;
	if (glm::length(probe) > 0.0f)
		probe = glm::normalize(probe) * (glm::length(probe) + extra);

	vec3 nextCamPos = camera.moveCheck(probe);
	float nextEyeY = -nextCamPos.y;
	int footCell = static_cast<int>(std::floor(nextEyeY - EYE_HEIGHT + EPS));
	int worldX = static_cast<int>(std::floor(-nextCamPos.x));
	int worldZ = static_cast<int>(std::floor(-nextCamPos.z));
	ivec3 worldPosFeet   = {worldX, footCell,         worldZ};
	ivec3 worldPosTorso  = {worldX, footCell + 1,     worldZ};
	// Derive chunk from integer world cell to avoid float-boundary errors
	ivec2 chunkPos = {
		static_cast<int>(std::floor(static_cast<float>(worldX) / static_cast<float>(CHUNK_SIZE))),
		static_cast<int>(std::floor(static_cast<float>(worldZ) / static_cast<float>(CHUNK_SIZE)))
	};

	// Check both the block in front of the legs and the upper body
	BlockType blockFeet  = _world.getBlock(chunkPos, worldPosFeet);
	BlockType blockTorso  = _world.getBlock(chunkPos, worldPosTorso);
	return ((blockFeet == AIR || blockFeet == WATER)
		&& (blockTorso == AIR || blockTorso == WATER));
}

// Divides every move into smaller steps if necessary typically if the player goes very
// fast to avoid block skips 
bool StoneEngine::tryMoveStepwise(const glm::vec3& moveVec, float stepSize)
{
	// Total distance to move
	float distance = glm::length(moveVec);
	if (distance <= 0.0f) return false;

	// Normalize and scale to step size
	glm::vec3 direction = glm::normalize(moveVec);
	float moved = 0.0f;

	while (moved < distance)
	{
		float step = std::min(stepSize, distance - moved);
		glm::vec3 offset = direction * step;

		if (!canMove(offset, 0.04f))
			return false;

		camera.move(offset);
		moved += step;
	}

	return true;
}

void StoneEngine::updatePlayerDirection()
{
	playerDir = {0};

	// Build direction once
	if (keyStates[GLFW_KEY_W]) playerDir.forward += moveSpeed;
	if (keyStates[GLFW_KEY_A]) playerDir.strafe += moveSpeed;
	if (keyStates[GLFW_KEY_S]) playerDir.forward += -moveSpeed;
	if (keyStates[GLFW_KEY_D]) playerDir.strafe += -moveSpeed;
	if (!gravity && keyStates[GLFW_KEY_SPACE]) playerDir.up += -moveSpeed;
	if (!gravity && keyStates[GLFW_KEY_LEFT_SHIFT]) playerDir.up += moveSpeed;
	if (jumping)
	{
		fallSpeed = 1.2f;
		playerDir.up += -moveSpeed;
		_jumpCooldown = now + std::chrono::milliseconds(400);
		jumping = false;
	}
}

void StoneEngine::updateMovement()
{
	vec3 oldPos = camera.getWorldPosition();
	glm::vec3 moveVec = camera.getForwardVector() * playerDir.forward
						+ camera.getStrafeVector()   * playerDir.strafe
						+ glm::vec3(0, 1, 0)        * playerDir.up;

	if (gravity)
	{
		float stepSize = 0.5f;

		glm::vec3 moveX(moveVec.x, 0.0f, 0.0f);
		glm::vec3 moveZ(0.0f, 0.0f, moveVec.z);
		glm::vec3 moveY(0.0f, moveVec.y, 0.0f);

		if (moveX.x != 0.0f) tryMoveStepwise(moveX, stepSize);
		if (moveZ.z != 0.0f) tryMoveStepwise(moveZ, stepSize);
		if (moveY.y != 0.0f) tryMoveStepwise(moveY, stepSize);
	}
	else
		camera.move(moveVec);

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
	timeValue += 6; // Increment time value per game tick
	//std::cout << "timeValue: " << timeValue << std::endl;

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
		glUniform1i(glGetUniformLocation(shaderProgram, "timeValue"), 58500);
		glUseProgram(postProcessShaders[GREEDYFIX].program);
		glUniform1i(glGetUniformLocation(postProcessShaders[GREEDYFIX].program, "timeValue"), 58500);
	}
	// Update gravity and falling values
	if (!swimming)
	{
		if (gravity && falling)
		{
			fallSpeed -= FALL_INCREMENT;
			fallSpeed *= 0.98;
		}
	}
	else
	{
		if (gravity && falling)
		{
			fallSpeed = -0.25;
		}
		if (keyStates[GLFW_KEY_SPACE] && std::chrono::steady_clock::now() > _swimUpCooldownOnRise)
		{
			fallSpeed += 0.75;
		}
	}
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
			else if(updateChunk && (floor(oldCamChunk.x) != floor(camChunk.x) || floor(oldCamChunk.y) != floor(camChunk.y)))
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
	double humidBias = (-continental * 0.5)              // wetter near coasts (low continentalness)
						+ ((1.0 - alt01) * 0.2)             // more humidity at low elevations
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
	// Check for delta and apply to move and rotation speeds
	findMoveRotationSpeed();

	// Get current time
	end = std::chrono::steady_clock::now();
	now = end;
	delta = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
	start = end; // Reset start time for next frame

	// Check if it's time to update the game tick (20 times per second)
	static auto lastGameTick = std::chrono::steady_clock::now();
	if (std::chrono::duration_cast<std::chrono::milliseconds>(end - lastGameTick).count() >= (1000 / 20))
	{
		updateGameTick();
		lastGameTick = end; // Reset game tick timer
	}

	// Update player states
	if (gravity)
	{
		updatePlayerStates();
	}
	else
	{
		vec3 worldPos = camera.getWorldPosition();
		int worldX = static_cast<int>(std::floor(worldPos.x));
		int worldZ = static_cast<int>(std::floor(worldPos.z));
		ivec2 chunkPos = {
			static_cast<int>(std::floor(static_cast<float>(worldX) / static_cast<float>(CHUNK_SIZE))),
			static_cast<int>(std::floor(static_cast<float>(worldZ) / static_cast<float>(CHUNK_SIZE)))
		};
		const float eyeBias = 0.30f;
		int eyeCellY = static_cast<int>(std::floor(worldPos.y - eyeBias));
		BlockType camHeadBlock = _world.getBlock(chunkPos, {worldX, eyeCellY, worldZ});
		isUnderWater = (camHeadBlock == WATER);
	}

	// Update player position and orientation
	updatePlayerDirection();
	updateMovement();
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


void StoneEngine::reshapeAction(int width, int height)
{
	glViewport(0, 0, width, height);
	glMatrixMode(GL_PROJECTION);

	windowHeight = height;
	windowWidth = width;
	resetFrameBuffers();
	float y = mapExpo(_fov, 1.0f, 90.0f, 10.0f, 0.1f);
	projectionMatrix = perspective(radians(_fov), float(width) / float(height), y, 9600.0f);
	glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, value_ptr(projectionMatrix));
}

void StoneEngine::reshape(GLFWwindow* window, int width, int height)
{
	StoneEngine *engine = static_cast<StoneEngine*>(glfwGetWindowUserPointer(window));

	if (engine) engine->reshapeAction(width, height);
}

void StoneEngine::keyAction(int key, int scancode, int action, int mods)
{
	(void)scancode;
	(void)mods;
	if (action == GLFW_PRESS && key == GLFW_KEY_F)
	{
		_fov = 80.0f;
		reshapeAction(windowWidth, windowHeight);
	}
	if (action == GLFW_PRESS && key == GLFW_KEY_C) updateChunk = !updateChunk;
	if (action == GLFW_PRESS && key == GLFW_KEY_G) gravity = !gravity;
	if (action == GLFW_PRESS && key == GLFW_KEY_F3) showDebugInfo = !showDebugInfo;
	if (action == GLFW_PRESS && key == GLFW_KEY_F4) showTriangleMesh = !showTriangleMesh;
	if (action == GLFW_PRESS && key == GLFW_KEY_L) showLight = !showLight;
	if (action == GLFW_PRESS && (key == GLFW_KEY_M || key == GLFW_KEY_SEMICOLON))
		mouseCaptureToggle = !mouseCaptureToggle;
	if (action == GLFW_PRESS && (key == GLFW_KEY_F5)) camera.invert();
	if (key == GLFW_KEY_ESCAPE) glfwSetWindowShouldClose(_window, GL_TRUE);
	if (action == GLFW_PRESS) keyStates[key] = true;
	else if (action == GLFW_RELEASE) keyStates[key] = false;
}

void StoneEngine::keyPress(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	StoneEngine *engine = static_cast<StoneEngine*>(glfwGetWindowUserPointer(window));

	if (engine) engine->keyAction(key, scancode, action, mods);
}

void StoneEngine::mouseAction(double x, double y)
{
	if (!mouseCaptureToggle)
	{
		glfwSetInputMode(_window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
		return ;
	}
	static bool firstMouse = true;
	static double lastX = 0, lastY = 0;

	// Get the current window size dynamically
	int windowWidth, windowHeight;
	glfwGetWindowSize(_window, &windowWidth, &windowHeight);
	glfwSetInputMode(_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

	camera.updateMousePos(x, y);

	int windowCenterX = windowWidth / 2;
	int windowCenterY = windowHeight / 2;

	if (firstMouse || ignoreMouseEvent)
	{
		lastX = windowCenterX;
		lastY = windowCenterY;
		firstMouse = false;
		ignoreMouseEvent = false;
		return;
	}

	float xOffset = lastX - x;
	float yOffset = lastY - y;

	lastX = x;
	lastY = y;

	float sensitivity = 0.05f;
	xOffset *= sensitivity;
	yOffset *= sensitivity;

	camera.rotate(1.0f, 0.0f, xOffset * ROTATION_SPEED);
	camera.rotate(0.0f, 1.0f, yOffset * ROTATION_SPEED);
	ignoreMouseEvent = true;
	glfwSetCursorPos(_window, windowCenterX, windowCenterY);
}

void StoneEngine::mouseCallback(GLFWwindow* window, double x, double y)
{
	StoneEngine *engine = static_cast<StoneEngine*>(glfwGetWindowUserPointer(window));

	if (engine) engine->mouseAction(x, y);
}

void StoneEngine::scrollAction(double yoffset)
{
	_fov -= (float)yoffset;
	_fov = std::clamp(_fov, 1.0f, 90.0f);
	reshapeAction(windowWidth, windowHeight);
}

void StoneEngine::scrollCallback(GLFWwindow *window, double xoffset, double yoffset)
{
	(void)xoffset;
	StoneEngine *engine = static_cast<StoneEngine*>(glfwGetWindowUserPointer(window));

	if (engine) engine->scrollAction(yoffset);
}

int StoneEngine::initGLFW()
{	
	glfwWindowHint(GLFW_DEPTH_BITS, 32); // Request 32-bit depth buffer
	// glfwWindowHint(GLFW_SAMPLES, 4);
	_window = glfwCreateWindow(windowWidth, windowHeight, "Not_ft_minecraft | FPS: 0", NULL, NULL);
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
	glfwMakeContextCurrent(_window);
	glfwSwapInterval(0);
	if (!isWSL())
		glfwSetCursorPosCallback(_window, mouseCallback);
	return 1;
}

void StoneEngine::initGLEW()
{
	GLenum err = glewInit();
	if (err != GLEW_OK)
	{
		std::cerr << "GLEW initialization failed: " << glewGetErrorString(err) << std::endl;
		return ;
	}
}

bool StoneEngine::getIsRunning()
{
	std::lock_guard<std::mutex> lockGuard(_isRunningMutex);
	return _isRunning;
}
