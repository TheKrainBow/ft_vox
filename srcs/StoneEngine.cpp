#include "StoneEngine.hpp"

bool isTransparent(char block)
{
	return block == AIR || block == WATER;
}

bool faceDisplayCondition(char blockToDisplay, char neighborBlock)
{
	return isTransparent(neighborBlock) && blockToDisplay != neighborBlock;
}

const char* getShaderVertPath(StoneEngine::ShaderType type)
{
	switch (type) {
	case StoneEngine::GREEDYFIX: return "shaders/postProcess/greedyMeshing.vert";
	case StoneEngine::FOG: return "shaders/postProcess/fog.vert";
	case StoneEngine::GODRAYS:   return "shaders/postProcess/godrays.vert";
	default:        return "";
	}
}

const char* getShaderFragPath(StoneEngine::ShaderType type)
{
	switch (type) {
	case StoneEngine::GREEDYFIX: return "shaders/postProcess/greedyMeshing.frag";
	case StoneEngine::FOG: return "shaders/postProcess/fog.frag";
	case StoneEngine::GODRAYS:   return "shaders/postProcess/godrays.frag";
	default:        return "";
	}
}

void StoneEngine::updateFboWindowSize(PostProcessShader &shader)
{
	float texelX = 1.0f / windowWidth;
	float texelY = 1.0f / windowHeight;

	GLint texelSizeLoc = glGetUniformLocation(shader.program, "texelSize");
	glUseProgram(shader.program);
	glUniform2f(texelSizeLoc, texelX, texelY);
}

StoneEngine::StoneEngine(int seed, ThreadPool &pool) : _world(seed, _textureManager, camera, pool), _pool(pool), noise_gen(seed)
{
	initData();
	initGLFW();
	initGLEW();
	initTextures();
	initRenderShaders();
	initDebugTextBox();
	initFboShaders();
	reshapeAction(windowWidth, windowHeight);
	_world.init(shaderProgram, RENDER_DISTANCE);
	_world.setRunning(&_isRunningMutex, &_isRunning);
}

StoneEngine::~StoneEngine()
{
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
	// Main loop
	_isRunning = true;
	std::future<void> chunkLoadThread = _pool.enqueue(&StoneEngine::updateChunkWorker, this);
	while (!glfwWindowShouldClose(_window))
	{
		glClear(GL_COLOR_BUFFER_BIT);
		update();
		glfwPollEvents();
	}
	_isRunningMutex.lock();
	_isRunning = false;
	_isRunningMutex.unlock();
	chunkLoadThread.get();
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
	});

	glGenTextures(1, &waterNormalMap);
	glBindTexture(GL_TEXTURE_2D, waterNormalMap);

	// Load your image here (example uses stb_image)
	int width, height, nrChannels;
	unsigned char* data = stbi_load("textures/water_normal.jpg", &width, &height, &nrChannels, 0);

	if (data) {
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0,
					GL_RGB, GL_UNSIGNED_BYTE, data);
		glGenerateMipmap(GL_TEXTURE_2D);
	} else {
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

    float angle = (timeValue / 86400.0f) * 2.0f * pi;

    // Orbit above the player (east-to-west arc)
    float x = radius * cos(angle);
    float y = -radius * sin(angle);
    float z = 0.0f;

    // Center the sun arc around the player
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
	glBindFramebuffer(GL_FRAMEBUFFER, writeFBO.fbo);
	// glDepthFunc(GL_LESS);
	vec3 camPos = camera.getWorldPosition();
	// Compute current sun position based on time
	glm::vec3 sunPos = computeSunPosition(timeValue, camPos);

	// Update view matrix
	glm::mat4 viewMatrix = glm::mat4(1.0f);
	float radX = camera.getAngles().x * (M_PI / 180.0f);
	float radY = camera.getAngles().y * (M_PI / 180.0f);

	viewMatrix = glm::rotate(viewMatrix, radY, glm::vec3(-1.0f, 0.0f, 0.0f));
	viewMatrix = glm::rotate(viewMatrix, radX, glm::vec3(0.0f, -1.0f, 0.0f));
	viewMatrix = glm::translate(viewMatrix, camera.getPosition());

	// Use sun shader
	glUseProgram(sunShaderProgram);

	// Set uniforms
	glUniformMatrix4fv(glGetUniformLocation(sunShaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(viewMatrix));
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
	createPostProcessShader(postProcessShaders[GREEDYFIX], "shaders/postProcess/greedyMeshing.vert", "shaders/postProcess/greedyMeshing.frag");
	createPostProcessShader(postProcessShaders[FOG], "shaders/postProcess/fog.vert", "shaders/postProcess/fog.frag");
}

void StoneEngine::initDebugTextBox()
{
	vec3 *camPos = camera.getPositionPtr();
	fvec2 *camAngle = camera.getAnglesPtr();
	e_direction *facing_direction = camera.getDirectionPtr();

	debugBox.initData(_window, 0, 0, 200, 200);
	debugBox.loadFont("textures/CASCADIAMONO.TTF", 20);
	debugBox.addLine("FPS: ", Textbox::DOUBLE, &fps);
	debugBox.addLine("Triangles: ", Textbox::DOUBLE, &drawnTriangles);
	debugBox.addLine("Memory Usage: ", Textbox::SIZE_T, _world.getMemorySizePtr());
	debugBox.addLine("RenderDistance: ", Textbox::INT, _world.getRenderDistancePtr());
	debugBox.addLine("CurrentRender: ", Textbox::INT, _world.getCurrentRenderPtr());
	debugBox.addLine("x: ", Textbox::FLOAT, &camPos->x);
	debugBox.addLine("y: ", Textbox::FLOAT, &camPos->y);
	debugBox.addLine("z: ", Textbox::FLOAT, &camPos->z);
	debugBox.addLine("xangle: ", Textbox::FLOAT, &camAngle->x);
	debugBox.addLine("yangle: ", Textbox::FLOAT, &camAngle->y);
	debugBox.addLine("time: ", Textbox::INT, &timeValue);
	debugBox.addLine("Facing: ", Textbox::DIRECTION, facing_direction);

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

	mat4 viewMatrix = mat4(1.0f);
	viewMatrix = rotate(viewMatrix, radY, vec3(-1.0f, 0.0f, 0.0f));
	viewMatrix = rotate(viewMatrix, radX, vec3(0.0f, -1.0f, 0.0f));
	viewMatrix = translate(viewMatrix, vec3(camera.getPosition()));

	glUseProgram(shaderProgram);
	glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, value_ptr(projectionMatrix));
	glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, value_ptr(modelMatrix));
	glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, value_ptr(viewMatrix));
	glUniform3fv(glGetUniformLocation(shaderProgram, "lightColor"), 1, value_ptr(vec3(1.0f, 0.95f, 0.95f)));

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D_ARRAY, _textureManager.getTextureArray());
	glUniform1i(glGetUniformLocation(shaderProgram, "textureArray"), 0);

	glEnable(GL_CULL_FACE);
	glCullFace(GL_FRONT);      // Cull back faces
	glFrontFace(GL_CCW);      // Set counter-clockwise as the front face
}

void StoneEngine::activateTransparentShader()
{
	mat4 modelMatrix = mat4(1.0f);
	mat4 viewMatrix = lookAt(
		camera.getWorldPosition(),                         // eye
		camera.getWorldPosition() + camera.getDirection(), // center
		vec3(0.0f, 1.0f, 0.0f)                         // up
	);

	vec3 viewPos = camera.getWorldPosition();

	glUseProgram(waterShaderProgram);

	// Uniforms for rendering
	glUniformMatrix4fv(glGetUniformLocation(waterShaderProgram, "projection"), 1, GL_FALSE, value_ptr(projectionMatrix));
	glUniformMatrix4fv(glGetUniformLocation(waterShaderProgram, "model"), 1, GL_FALSE, value_ptr(modelMatrix));
	glUniformMatrix4fv(glGetUniformLocation(waterShaderProgram, "view"), 1, GL_FALSE, value_ptr(viewMatrix));
	glUniform3fv(glGetUniformLocation(waterShaderProgram, "viewPos"), 1, value_ptr(viewPos));
	glUniform1f(glGetUniformLocation(waterShaderProgram, "time"), timeValue);
	glUniform1i(glGetUniformLocation(waterShaderProgram, "isUnderwater"), viewPos.y <= OCEAN_HEIGHT + 1 ? 1 : 0);

	// SSR-related matrices
	glUniformMatrix4fv(glGetUniformLocation(waterShaderProgram, "inverseProjection"), 1, GL_FALSE, value_ptr(inverse(projectionMatrix)));
	glUniformMatrix4fv(glGetUniformLocation(waterShaderProgram, "inverseView"), 1, GL_FALSE, value_ptr(inverse(viewMatrix)));
	glUniform2fv(glGetUniformLocation(waterShaderProgram, "screenSize"), 1, value_ptr(glm::vec2(windowWidth, windowHeight)));

	// TEXTURES
	// Bind screen color texture
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, readFBO.texture);
	glUniform1i(glGetUniformLocation(waterShaderProgram, "screenTexture"), 0);

	// Bind depth texture
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, readFBO.depth);
	glUniform1i(glGetUniformLocation(waterShaderProgram, "depthTexture"), 1);

	// Bind normal map
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, waterNormalMap);
	glUniform1i(glGetUniformLocation(waterShaderProgram, "normalMap"), 2);

	// Blending and depth settings
	glDepthMask(GL_FALSE);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_FRONT);
	glFrontFace(GL_CCW);
}

void StoneEngine::display() {
    prepareRenderPipeline();

    renderSceneToFBO();
	screenshotFBOBuffer();

    postProcessGreedyFix();
    displaySun();
    screenshotFBOBuffer();

    renderTransparentObjects();
    screenshotFBOBuffer();

    postProcessFog();
    displaySun(); // Redisplay sun because FOG erased it

    sendPostProcessFBOToDispay();
    renderOverlayAndUI();
    finalizeFrame();
}

void StoneEngine::postProcessFog()
{
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

	// Bind depth texture (you may need to track this separately or store it in the struct)
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, readFBO.depth);  // If shared, keep using dboTexture
	glUniform1i(glGetUniformLocation(shader.program, "depthTexture"), 1);

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

	// Bind depth texture (you may need to track this separately or store it in the struct)
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, readFBO.depth);  // If shared, keep using dboTexture
	glUniform1i(glGetUniformLocation(shader.program, "depthTexture"), 1);

	// Set uniforms (after glUseProgram!)
	glUniform1i(glGetUniformLocation(shader.program, "isUnderwater"), camera.getWorldPosition().y <= OCEAN_HEIGHT + 1 ? 1 : 0);
	glUniform1f(glGetUniformLocation(shader.program, "waterHeight"), OCEAN_HEIGHT + 2);
	glUniform3fv(glGetUniformLocation(shader.program, "viewPos"), 1, glm::value_ptr(camera.getWorldPosition()));

	glm::vec3 sunPosition = computeSunPosition(timeValue, camera.getWorldPosition());
	glUniform3fv(glGetUniformLocation(shader.program, "sunPosition"), 1, glm::value_ptr(sunPosition));
	
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	glDepthFunc(GL_LESS);
}

void StoneEngine::prepareRenderPipeline() {
    if (showTriangleMesh) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    } else {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glBindFramebuffer(GL_FRAMEBUFFER, writeFBO.fbo);
    }

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glMatrixMode(GL_MODELVIEW);
}

void StoneEngine::renderSceneToFBO() {
    activateRenderShader();
    _world.updateDrawData();
    drawnTriangles = _world.display();
}

void StoneEngine::screenshotFBOBuffer() {
	// Copy from current write to current read
	glBindFramebuffer(GL_READ_FRAMEBUFFER, writeFBO.fbo);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, readFBO.fbo);
	glBlitFramebuffer(0, 0, windowWidth, windowHeight,
					  0, 0, windowWidth, windowHeight,
					  GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT, GL_NEAREST);

	// Now swap the pointers
	// std::swap(readFBO, writeFBO);

	// Clear the new write buffer
	// glBindFramebuffer(GL_FRAMEBUFFER, writeFBO.fbo);
	// glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void StoneEngine::sendPostProcessFBOToDispay() {
	// std::swap(readFBO, writeFBO);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, writeFBO.fbo);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
	glBlitFramebuffer(0, 0, windowWidth, windowHeight,
						0, 0, windowWidth, windowHeight,
						GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT, GL_NEAREST);
}

void StoneEngine::renderTransparentObjects() {
    glBindFramebuffer(GL_FRAMEBUFFER, writeFBO.fbo);
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
	chronoHelper.startChrono(0, "Load chunks");
	_world.loadFirstChunks(camera.getChunkPosition(CHUNK_SIZE));
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
	chronoHelper.printChronos();
}

void StoneEngine::findMoveRotationSpeed()
{
	// Calculate delta time
	static auto lastTime = std::chrono::steady_clock::now();
	auto currentTime = std::chrono::steady_clock::now();
	std::chrono::duration<float> elapsedTime = currentTime - lastTime;
	float deltaTime = std::min(elapsedTime.count(), 0.1f);
	lastTime = currentTime;


	// Apply delta to rotation and movespeed
	if (keyStates[GLFW_KEY_LEFT_CONTROL])
		moveSpeed = (MOVEMENT_SPEED * 20.0) * deltaTime;
	else
		moveSpeed = MOVEMENT_SPEED * deltaTime;
	

	//ZOOM
	if (keyStates[GLFW_KEY_KP_ADD]) {timeValue+=50;}
	if (keyStates[GLFW_KEY_KP_SUBTRACT]) {timeValue-=50;}

	if (!isWSL())
		rotationSpeed = (ROTATION_SPEED - 1.5) * deltaTime;
	else
		rotationSpeed = ROTATION_SPEED * deltaTime;

	start = std::chrono::steady_clock::now();
}

void StoneEngine::updateMovement()
{
	// Camera movement
	vec3 oldPos = camera.getWorldPosition(); // Old pos
	if (keyStates[GLFW_KEY_W]) camera.move(moveSpeed, 0.0, 0.0);
	if (keyStates[GLFW_KEY_A]) camera.move(0.0, moveSpeed, 0.0);
	if (keyStates[GLFW_KEY_S]) camera.move(-moveSpeed, 0.0, 0.0);
	if (keyStates[GLFW_KEY_D]) camera.move(0.0, -moveSpeed, 0.0);
	if (keyStates[GLFW_KEY_SPACE]) camera.move(0.0, 0.0, -moveSpeed);
	if (keyStates[GLFW_KEY_LEFT_SHIFT]) camera.move(0.0, 0.0, moveSpeed);
	vec3 viewPos = camera.getWorldPosition(); // New position

	if (viewPos != oldPos)
	{
		glUseProgram(shaderProgram);
		glUniform3fv(glGetUniformLocation(shaderProgram, "viewPos"), 1, value_ptr(viewPos));
	}

	// Camera rotation
	if (!isWSL())
	{
		if (keyStates[GLFW_KEY_UP]) camera.rotate(0.0f, -1.0f, rotationSpeed * 150.0f);
		if (keyStates[GLFW_KEY_DOWN]) camera.rotate(0.0f, 1.0f, rotationSpeed * 150.0f);
		if (keyStates[GLFW_KEY_RIGHT]) camera.rotate(1.0f, 0.0f, rotationSpeed * 150.0f);
		if (keyStates[GLFW_KEY_LEFT]) camera.rotate(-1.0f, 0.0f, rotationSpeed * 150.0f);
	}
	else
	{
		if (keyStates[GLFW_KEY_UP]) camera.rotate(0.0f, 1.0f, rotationSpeed);
		if (keyStates[GLFW_KEY_DOWN]) camera.rotate(0.0f, -1.0f, rotationSpeed);
		if (keyStates[GLFW_KEY_RIGHT]) camera.rotate(-1.0f, 0.0f, rotationSpeed);
		if (keyStates[GLFW_KEY_LEFT]) camera.rotate(1.0f, 0.0f, rotationSpeed);
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
	//std::cout << "Updating gameTICK" << std::endl;
}

void StoneEngine::update()
{
    // Check for delta and apply to move and rotation speeds
    findMoveRotationSpeed();

    // Get current time
    end = std::chrono::steady_clock::now();
    delta = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    start = end; // Reset start time for next frame

    // Check if it's time to update the game tick (20 times per second)
    static auto lastGameTick = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::milliseconds>(end - lastGameTick).count() >= (1000 / 20))
    {
        updateGameTick();
        lastGameTick = end; // Reset game tick timer
    }

    // Update player position and orientation
    updateMovement();
    display();
}

void StoneEngine::resetFrameBuffers()
{
	glDeleteFramebuffers(1, &readFBO.fbo);
	glDeleteTextures(1, &readFBO.texture);
	glDeleteTextures(1, &readFBO.depth);
	glDeleteFramebuffers(1, &writeFBO.fbo);
	glDeleteTextures(1, &writeFBO.texture);
	glDeleteTextures(1, &writeFBO.depth);
	initFramebuffers(readFBO, windowWidth, windowHeight);
	initFramebuffers(writeFBO, windowWidth, windowHeight);
}

float mapRange(float x, float in_min, float in_max, float out_min, float out_max) {
    return out_max - (x - in_min) * (out_max - out_min) / (in_max - in_min);
}

float mapExpo(float x, float in_min, float in_max, float out_min, float out_max) {
    float t = (x - in_min) / (in_max - in_min); // normalize to [0, 1]
    return out_min * std::pow(out_max / out_min, t); // exponential interpolation
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