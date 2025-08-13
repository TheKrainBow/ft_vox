#include "StoneEngine.hpp"

bool isTransparent(char block)
{
	return block == AIR || block == WATER;
}

bool faceDisplayCondition(char blockToDisplay, char neighborBlock)
{
	return isTransparent(neighborBlock) && blockToDisplay != neighborBlock;
}

StoneEngine::StoneEngine(int seed, ThreadPool &pool) : camera(), _world(seed, _textureManager, camera, pool), _pool(pool), noise_gen(seed)
{
	initData();
	initGLFW();
	initGLEW();
	initTextures();
	initRenderShaders();
	initDebugTextBox();
	initFramebuffers();
	initFboShaders();
	updateFboWindowSize();
	reshapeAction(windowWidth, windowHeight);
	_world.init(RENDER_DISTANCE);
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
	gravity				= GRAVITY;
	falling				= FALLING;
	swimming			= SWIMMING;
	jumping				= JUMPING;
	_jumpCooldown		= std::chrono::steady_clock::now();

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
	
	projectionMatrix = perspective(radians(80.0f), (float)W_WIDTH / (float)W_HEIGHT, 0.1f, 1000.0f);
	vec3 lightColor(1.0f, 1.0f, 1.0f);
	vec3 sunColor(1.0f, 0.7f, 1.0f);
	vec3 viewPos = camera.getWorldPosition();
	sunPosition = {0.0f, 0.0f, 0.0f};

	glUseProgram(shaderProgram);
	glUniform1i(glGetUniformLocation(shaderProgram, "useTexture"), GL_FALSE);  // Use texture unit 0
	glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, value_ptr(projectionMatrix));
	glUniform3fv(glGetUniformLocation(shaderProgram, "lightColor"), 1, value_ptr(lightColor));
	glUniform3fv(glGetUniformLocation(shaderProgram, "viewPos"), 1, value_ptr(viewPos));

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

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D_ARRAY, _textureManager.getTextureArray());
	glUniform1i(glGetUniformLocation(shaderProgram, "textureArray"), 0);

	glEnable(GL_CULL_FACE);
	glCullFace(GL_FRONT);      // Cull back faces
	glFrontFace(GL_CCW);      // Set counter-clockwise as the front face
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
	// Skip FBO, draw to screen (wireframe dependant)
    if (showTriangleMesh)
        glBindFramebuffer(GL_FRAMEBUFFER, 0); 
    else
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);

	// Clear depth and color buffers
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glMatrixMode(GL_MODELVIEW);

    // Render world ("better.vert/frag" shaders active)
    activateRenderShader();

	// Wireframe mode
    triangleMeshToggle();

	// Swap draw data with ready data
	_world.updateDrawData();

	// One draw call solid blocks
    drawnTriangles = _world.display();
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glDisable(GL_CULL_FACE);

    // Skip post-process only if not in wireframe
    if (!showTriangleMesh)
	{
		// Post processing for T-junction holes ("fbo.vert/frag" shaders active )
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glClear(GL_COLOR_BUFFER_BIT);
        activateFboShader();

        glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        glBlitFramebuffer(0, 0, windowWidth, windowHeight,
                        0, 0, windowWidth, windowHeight,
                        GL_DEPTH_BUFFER_BIT, GL_NEAREST);
		// Deactivate post processing for water
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    // Transparency settings & UI
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_DEPTH_TEST);
	
    // Render transparent world ("better.vert/frag" shaders active)
    activateRenderShader();

	// One draw call transparent blocks
    drawnTriangles += _world.displayTransparent();
	glDisable(GL_CULL_FACE);

	// Deactivating lines for debug console
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    if (showDebugInfo)
        debugBox.render();
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

ivec2 StoneEngine::getChunkPos(ivec2 pos)
{
	ivec2 camChunk(-pos.x / CHUNK_SIZE, -pos.y / CHUNK_SIZE);
	if (-pos.x < 0) camChunk.x--;
	if (-pos.y < 0) camChunk.y--;
	return camChunk;
}

bool StoneEngine::canMove(const glm::vec3& offset, float extra)
{
	glm::vec3 probe = offset;
	if (glm::length(probe) > 0.0f)
		probe = glm::normalize(probe) * (glm::length(probe) + extra);

	ivec3 nextCamPos = camera.moveCheck(probe);
	ivec3 worldPos   = {-nextCamPos.x, -(nextCamPos.y + 2), -nextCamPos.z};
	ivec2 chunkPos   = getChunkPos({nextCamPos.x, nextCamPos.z});
	BlockType block  = _world.getBlock(chunkPos, worldPos);
	return block == AIR || block == WATER;
}

void StoneEngine::updateFalling(vec3 worldPos, int blockHeight)
{
	if (!falling && worldPos.y > blockHeight + 3) falling = true;
	if (falling && worldPos.y <= blockHeight + 3)
	{
		falling = false;
		fallSpeed = 0.0;
		camera.setPos({-worldPos.x, -(blockHeight + 3), -worldPos.z});
	}
	camera.move({0.0, -(fallSpeed * deltaTime), 0.0});
}

void StoneEngine::updateSwimming(BlockType block)
{
	if (!swimming && block == WATER)
	{
		swimming = true;
		if (!keyStates[GLFW_KEY_SPACE])
			fallSpeed = -0.5;
		else
			fallSpeed = 0.0;
	}
	if (swimming && block != WATER)
	{
		swimming = false;
		fallSpeed = 0.0;
	}
}

void StoneEngine::updateJumping()
{
	jumping = gravity && !falling && keyStates[GLFW_KEY_SPACE] && now > _jumpCooldown && !swimming;
}

void StoneEngine::updatePlayerStates()
{
	vec3 worldPos = camera.getWorldPosition();
	ivec2 chunkPos = camera.getChunkPosition(CHUNK_SIZE);
	BlockType camStandingBlock = AIR;
	BlockType camBodyBlockLegs = AIR;
	BlockType camBodyBlockTorso = AIR;

	camTopBlock = _world.findTopBlockY(chunkPos, {worldPos.x, worldPos.z});
	camStandingBlock = _world.getBlock(chunkPos, {worldPos.x, worldPos.y - 3, worldPos.z});
	camBodyBlockLegs = _world.getBlock(chunkPos, {worldPos.x, worldPos.y - 2, worldPos.z});
	camBodyBlockTorso = _world.getBlock(chunkPos, {worldPos.x, worldPos.y - 1, worldPos.z});
	int blockHeight = camTopBlock.height;
	BlockType inWater = (camStandingBlock == WATER
						|| camBodyBlockLegs == WATER
						|| camBodyBlockTorso == WATER) ? WATER : AIR;
	
	updateFalling(worldPos, blockHeight);
	updateSwimming(inWater);
	updateJumping();
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
		_jumpCooldown = now + std::chrono::milliseconds(500);
		jumping = false;
	}
	// else if (gravity && grounded && keyStates[GLFW_KEY_SPACE] && swimming)
	// {
	// 	fallSpeed = 1.2f;
	// 	playerDir.up += -(moveSpeed * 0.01);
	// }
}

void StoneEngine::updateMovement()
{
	// Camera position
	vec3 oldPos = camera.getWorldPosition();
	glm::vec3 moveVec = camera.getForwardVector() * playerDir.forward
					+ camera.getStrafeVector()   * playerDir.strafe
					+ glm::vec3(0, 1, 0)        * playerDir.up;

	// Move for each axis
	glm::vec3 moveX(moveVec.x, 0.0f, 0.0f);
	glm::vec3 moveZ(0.0f, 0.0f, moveVec.z);
	glm::vec3 moveY(0.0f, moveVec.y, 0.0f);

	// Checks to be able to move
	if (gravity)
	{
		float margin = 0.04f;

		if (moveX.x != 0.0f && canMove(moveX, margin))
			camera.move(moveX);

		if (moveZ.z != 0.0f && canMove(moveZ, margin))
			camera.move(moveZ);

		if (moveY.y != 0.0f && canMove(moveY, margin))
			camera.move(moveY);
	}
	else camera.move(moveVec);
	vec3 newPos = camera.getWorldPosition();

	// If the player changed position, update the shader
	if (newPos != oldPos)
	{
		glUseProgram(shaderProgram);
		glUniform3fv(glGetUniformLocation(shaderProgram, "viewPos"), 1, value_ptr(newPos));
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
	}
	else
	{
		// Always daytime
		glUseProgram(shaderProgram);
		glUniform1i(glGetUniformLocation(shaderProgram, "timeValue"), 58500);
		glUseProgram(fboShaderProgram);
		glUniform1i(glGetUniformLocation(fboShaderProgram, "timeValue"), 58500);
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
			fallSpeed = -0.5;
		}
		if (keyStates[GLFW_KEY_SPACE]) fallSpeed += 1.0;
	}
	//std::cout << "Updating gameTICK" << std::endl;
}

void StoneEngine::update()
{
	// Check for delta and apply to move and rotation speeds
	findMoveRotationSpeed();

	// Get current time
	now = std::chrono::steady_clock::now();
	delta = std::chrono::duration_cast<std::chrono::milliseconds>(now - start);
	start = now; // Reset start time for next frame

	// Check if it's time to update the game tick (20 times per second)
	static auto lastGameTick = std::chrono::steady_clock::now();
	if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastGameTick).count() >= (1000 / 20))
	{
		updateGameTick();
		lastGameTick = now; // Reset game tick timer
	}

	// Update player states
	if (gravity)
		updatePlayerStates();

	// Update player position and orientation
	updatePlayerDirection();
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
