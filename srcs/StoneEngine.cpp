#include "StoneEngine.hpp"

bool isTransparent(char block)
{
	return block == AIR || block == WATER;
}

bool faceDisplayCondition(char blockToDisplay, char neighbourBlock)
{
	return isTransparent(neighbourBlock) && blockToDisplay != neighbourBlock;
}

StoneEngine::StoneEngine(int seed) : _world(seed, _textureManager, camera), noise_gen(seed)
{
	initData();
	initGLFW();
	initGLEW();
	initTextures();
	initRenderShaders();
	initDebugTextBox();
	initFramebuffers();
	initFboShaders();
	initSunShaders();
	updateFboWindowSize();
	reshape(_window, windowWidth, windowHeight);
	_world.setRunning(&_isRunningMutex, &_isRunning);
}

StoneEngine::~StoneEngine()
{
	glDeleteProgram(shaderProgram);
	glDeleteFramebuffers(1, &fbo);
	glDeleteTextures(1, &fboTexture);
	glDeleteTextures(1, &dboTexture);
	glfwDestroyWindow(_window);
	glfwTerminate();
}

void StoneEngine::run()
{
	// Main loop
	_isRunning = true;
	std::thread t1(&StoneEngine::updateChunkWorker, this);
	while (!glfwWindowShouldClose(_window))
	{
		glClear(GL_COLOR_BUFFER_BIT);
		update();
		glfwPollEvents();
	}
	_isRunningMutex.lock();
	_isRunning = false;
	_isRunningMutex.unlock();
	// Terminate chunk thread
	t1.join();
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

void StoneEngine::initFramebuffers()
{
	// Init framebuffer
	glGenFramebuffers(1, &fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);

	// Init framebuffer color texture
	glGenTextures(1, &fboTexture);
	glBindTexture(GL_TEXTURE_2D, fboTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, windowWidth, windowHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fboTexture, 0);

	// Init render buffer
	// glGenRenderbuffers(1, &rbo);
	// glBindRenderbuffer(GL_RENDERBUFFER, rbo);
	// glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, windowWidth, windowHeight);
	// glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo);

	glGenTextures(1, &dboTexture);
	glBindTexture(GL_TEXTURE_2D, dboTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, windowWidth, windowHeight, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	// Attach to framebuffer
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, dboTexture, 0);

	GLuint fboStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if (fboStatus != GL_FRAMEBUFFER_COMPLETE)
		std::cout << "Framebuffer error: " << fboStatus << std::endl;
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
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
}

void StoneEngine::initRenderShaders()
{
	shaderProgram = createShaderProgram("shaders/better.vert", "shaders/better.frag");
	
	projectionMatrix = glm::perspective(glm::radians(80.0f), (float)W_WIDTH / (float)W_HEIGHT, 0.1f, 10000000.0f);
	glm::vec3 lightColor(1.0f, 1.0f, 1.0f);
	glm::vec3 viewPos = camera.getWorldPosition();

	glUseProgram(shaderProgram);
	glUniform1i(glGetUniformLocation(shaderProgram, "useTexture"), GL_FALSE);  // Use texture unit 0
	glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projectionMatrix));
	glUniform3fv(glGetUniformLocation(shaderProgram, "lightColor"), 1, glm::value_ptr(lightColor));
	glUniform3fv(glGetUniformLocation(shaderProgram, "viewPos"), 1, glm::value_ptr(viewPos));

	glBindTexture(GL_TEXTURE_2D, _textureManager.getTextureArray());  // Bind the texture
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);
}

void StoneEngine::initFboShaders()
{
	// Vao and Vbo that covers the whole screen basically a big rectangle of screen size
	glGenVertexArrays(1, &rectangleVao);
	glGenBuffers(1, &rectangleVbo);
	glBindVertexArray(rectangleVao);
	glBindBuffer(GL_ARRAY_BUFFER, rectangleVbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(rectangleVertices), &rectangleVertices, GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)(2 * sizeof(float)));

	fboShaderProgram = createShaderProgram("shaders/fbo.vert", "shaders/fbo.frag");
	glUseProgram(fboShaderProgram);
	glUniform1i(glGetUniformLocation(fboShaderProgram, "screenTexture"), 0);
	glUniform1i(glGetUniformLocation(fboShaderProgram, "depthTexture"), 1);
}

void StoneEngine::initDebugTextBox()
{
	vec3 *camPos = camera.getPositionPtr();
	vec2 *camAngle = camera.getAnglesPtr();
	e_direction *facing_direction = camera.getDirectionPtr();

	debugBox.initData(_window, 0, 0, 200, 200);
	debugBox.loadFont("textures/CASCADIAMONO.TTF", 20);
	debugBox.addLine("FPS: ", Textbox::DOUBLE, &fps);
	debugBox.addLine("Triangles: ", Textbox::DOUBLE, &drawnTriangles);
	debugBox.addLine("RenderDistance: ", Textbox::INT, _world.getRenderDistancePtr());
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
	glm::mat4 modelMatrix = glm::mat4(1.0f);
	float radY, radX;
	radX = camera.getAngles().x * (M_PI / 180.0);
	radY = camera.getAngles().y * (M_PI / 180.0);

	glm::mat4 viewMatrix = glm::mat4(1.0f);
	viewMatrix = glm::rotate(viewMatrix, radY, glm::vec3(-1.0f, 0.0f, 0.0f));
	viewMatrix = glm::rotate(viewMatrix, radX, glm::vec3(0.0f, -1.0f, 0.0f));
	viewMatrix = glm::translate(viewMatrix, glm::vec3(camera.getPosition()));

	glUseProgram(shaderProgram);
	glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projectionMatrix));
	glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(modelMatrix));
	glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(viewMatrix));

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D_ARRAY, _textureManager.getTextureArray());
	glUniform1i(glGetUniformLocation(shaderProgram, "textureArray"), 0);
	
	if (showTriangleMesh)
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	else
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	glEnable(GL_CULL_FACE);
	glCullFace(GL_FRONT);      // Cull back faces
	glFrontFace(GL_CCW);      // Set counter-clockwise as the front face
}

void StoneEngine::initSunShaders()
{
    sunProgram = createShaderProgram("shaders/sun.vert", "shaders/sun.frag");
    glm::vec3 sunColor(1.0f, 0.7f, 0.2f);  // Yellow-ish sun color
    glm::vec3 viewPos = camera.getWorldPosition();

    // Vertices for a square (two triangles)
    GLfloat sunVertices[] = {
        -0.1f,  0.1f, 0.0f, // Top-left
        -0.1f, -0.1f, 0.0f, // Bottom-left
        0.1f,  0.1f, 0.0f, // Top-right
        0.1f, -0.1f, 0.0f  // Bottom-right
    };

    glUseProgram(sunProgram);
    glUniform3fv(glGetUniformLocation(sunProgram, "sunColor"), 1, glm::value_ptr(sunColor));
    glUniform3fv(glGetUniformLocation(sunProgram, "viewPos"), 1, glm::value_ptr(viewPos));

    glGenVertexArrays(1, &sunVAO);
    glBindVertexArray(sunVAO);

    glGenBuffers(1, &sunVBO);
    glBindBuffer(GL_ARRAY_BUFFER, sunVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(sunVertices), sunVertices, GL_STATIC_DRAW);

    // Position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GL_FLOAT), (GLvoid*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);  // Unbind VAO
}

void StoneEngine::renderSun()
{
	glUseProgram(sunProgram);

	// Compute the sun's position based on time
	float sunAngle = (timeValue / 86400.0f) * (2 * M_PI); // Full circle over 24 hours
	glm::vec3 sunPos(
		100.0f * sin(sunAngle),  // X position
		100.0f * sin(sunAngle),  // Y position (can be adjusted)
		100.0f * cos(sunAngle)   // Z position
	);

	// Set the sun position uniform
	glUniform3fv(glGetUniformLocation(sunProgram, "sunPos"), 1, glm::value_ptr(sunPos));

	// Set up the model, view, and projection matrices
	// Identity matrix for simple position
	glm::mat4 model = glm::mat4(1.0f); 
	glm::mat4 viewMatrix = glm::mat4(1.0f);
	float radY, radX;
	radX = camera.getAngles().x * (M_PI / 180.0);
	radY = camera.getAngles().y * (M_PI / 180.0);
	viewMatrix = glm::rotate(viewMatrix, radY, glm::vec3(-1.0f, 0.0f, 0.0f));
	viewMatrix = glm::rotate(viewMatrix, radX, glm::vec3(0.0f, -1.0f, 0.0f));
	viewMatrix = glm::translate(viewMatrix, glm::vec3(camera.getPosition()));

	glUniformMatrix4fv(glGetUniformLocation(sunProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));
	glUniformMatrix4fv(glGetUniformLocation(sunProgram, "view"), 1, GL_FALSE, glm::value_ptr(viewMatrix));
	glUniformMatrix4fv(glGetUniformLocation(sunProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projectionMatrix));

	// Render the sun (as a simple quad in this case)
	glBindVertexArray(sunVAO);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);  // Draw the square
	glBindVertexArray(0);
}


void StoneEngine::activateFboShader()
{
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glUseProgram(fboShaderProgram);
	glBindVertexArray(rectangleVao);
	glDisable(GL_DEPTH_TEST);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, fboTexture);
	glUniform1i(glGetUniformLocation(fboShaderProgram, "screenTexture"), 0);
	
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, dboTexture);
	glUniform1i(glGetUniformLocation(fboShaderProgram, "depthTexture"), 1);
	glDrawArrays(GL_TRIANGLES, 0, 6);
}

void StoneEngine::triangleMeshToggle()
{
	if (showTriangleMesh)
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	else
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}
void StoneEngine::display()
{
    // Init and clear buffers
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glMatrixMode(GL_MODELVIEW);

	// Render solid blocks
    activateRenderShader();
    _world.updateActiveChunks();
    glCullFace(GL_FRONT);
    glFrontFace(GL_CCW);
    drawnTriangles = _world.display();
    glDisable(GL_CULL_FACE);
    triangleMeshToggle();

	renderSun();

    // Fix holes in terrain with post process fbo.frag shader
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glClear(GL_COLOR_BUFFER_BIT);
    activateFboShader();

    // Copy dbo from screen
    glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glBlitFramebuffer(0, 0, windowWidth, windowHeight,
                      0, 0, windowWidth, windowHeight,
                      GL_DEPTH_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Transparent blocks draw
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_DEPTH_TEST);

    activateRenderShader();
    drawnTriangles += _world.displayTransparent();

    // Debug UI
    if (showDebugInfo)
        debugBox.render();

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);

    // Swap buffers
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
	chronoHelper.startChrono(0, "Load chunks");

	// Run both calls in parallel using std::async
	// auto unLoadNextChunk = std::async(std::launch::async, 
	// 	[&]() {_world.unLoadNextChunks(oldCamChunk, newCamChunk, worldPos); });

	// auto loadNextChunk = std::async(std::launch::async, 
	// 	[&]() { _world.loadFirstChunks(worldPos); });

	// Ensure both complete before stopping the chrono
	// loadNextChunk.get();
	// unLoadNextChunk.get();
	_world.loadFirstChunks(newCamChunk);
	_world.unLoadNextChunks(newCamChunk);
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
	glm::vec3 oldPos = camera.getWorldPosition(); // Old pos
	if (keyStates[GLFW_KEY_W]) camera.move(moveSpeed, 0.0, 0.0);
	if (keyStates[GLFW_KEY_A]) camera.move(0.0, moveSpeed, 0.0);
	if (keyStates[GLFW_KEY_S]) camera.move(-moveSpeed, 0.0, 0.0);
	if (keyStates[GLFW_KEY_D]) camera.move(0.0, -moveSpeed, 0.0);
	if (keyStates[GLFW_KEY_SPACE]) camera.move(0.0, 0.0, -moveSpeed);
	if (keyStates[GLFW_KEY_LEFT_SHIFT]) camera.move(0.0, 0.0, moveSpeed);
	glm::vec3 viewPos = camera.getWorldPosition(); // New position

	if (viewPos != oldPos)
	{
		glUseProgram(shaderProgram);
		glUniform3fv(glGetUniformLocation(shaderProgram, "viewPos"), 1, glm::value_ptr(viewPos));
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
		glUseProgram(fboShaderProgram);
		glUniform1i(glGetUniformLocation(fboShaderProgram, "timeValue"), timeValue);
		// glUseProgram(sunProgram);
		// glUniform1i(glGetUniformLocation(sunProgram, "timeValue"), timeValue);
	}
	else
	{
		// Always daytime
		glUseProgram(shaderProgram);
		glUniform1i(glGetUniformLocation(shaderProgram, "timeValue"), 58500);
		glUseProgram(fboShaderProgram);
		glUniform1i(glGetUniformLocation(fboShaderProgram, "timeValue"), 58500);
		// glUseProgram(sunProgram);
		// glUniform1i(glGetUniformLocation(sunProgram, "timeValue"), 58500);
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

void StoneEngine::updateFboWindowSize()
{
	float texelX = 1.0f / windowWidth;
	float texelY = 1.0f / windowHeight;

	GLint texelSizeLoc = glGetUniformLocation(fboShaderProgram, "texelSize");
	glUseProgram(fboShaderProgram);
	glUniform2f(texelSizeLoc, texelX, texelY);
}

void StoneEngine::resetFrameBuffers()
{
	// Delete old framebuffer and attachments
	glDeleteFramebuffers(1, &fbo);
	glDeleteTextures(1, &fboTexture);
	glDeleteTextures(1, &dboTexture);
	initFramebuffers();
	updateFboWindowSize();
}

void StoneEngine::reshapeAction(int width, int height)
{
	glViewport(0, 0, width, height);
	glMatrixMode(GL_PROJECTION);

	windowHeight = height;
	windowWidth = width;
	resetFrameBuffers();
	projectionMatrix = glm::perspective(glm::radians(80.0f), float(width) / float(height), 0.1f, 10000000.0f);
	glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projectionMatrix));
	glLoadMatrixf(glm::value_ptr(projectionMatrix));
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
	if (action == GLFW_PRESS && key == GLFW_KEY_C) updateChunk = !updateChunk;
	if (action == GLFW_PRESS && key == GLFW_KEY_F3) showDebugInfo = !showDebugInfo;
	if (action == GLFW_PRESS && key == GLFW_KEY_F4) showTriangleMesh = !showTriangleMesh;
	if (action == GLFW_PRESS && key == GLFW_KEY_L) showLight = !showLight;
	if (action == GLFW_PRESS && (key == GLFW_KEY_M || key == GLFW_KEY_SEMICOLON))
		mouseCaptureToggle = !mouseCaptureToggle;
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