#include "StoneEngine.hpp"

StoneEngine::StoneEngine(int seed) : _world(seed, _textureManager, camera), noise_gen(seed)//, updateChunkFlag(false), running(true)
{
	initData();
	initGLFW();
	initGLEW();
	initTextures();
	initShaders();
	initDebugTextBox();
	reshape(_window, windowWidth, windowHeight);
	_world.setRunning(&_isRunningMutex, &_isRunning);
}

StoneEngine::~StoneEngine()
{
	glDeleteProgram(shaderProgram);
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
	});
}

void StoneEngine::initShaders()
{
	shaderProgram = createShaderProgram("shaders/better.vert", "shaders/better.frag");
	glm::mat4 projectionMatrix = glm::perspective(glm::radians(80.0f), (float)W_WIDTH / (float)W_HEIGHT, 0.1f, 10000000.0f);

	glUseProgram(shaderProgram);
	glUniform1i(glGetUniformLocation(shaderProgram, "useTexture"), GL_FALSE);  // Use texture unit 0
	glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projectionMatrix));

	glBindTexture(GL_TEXTURE_2D, _textureManager.getTextureArray());  // Bind the texture
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);
}

void StoneEngine::initDebugTextBox()
{
	vec3 *camPos = camera.getPositionPtr();
	vec2 *camAngle = camera.getAnglesPtr();

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
	glClearColor(0.53f, 0.81f, 0.92f, 1.0f); // Soft sky blue
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

void StoneEngine::display()
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glMatrixMode(GL_MODELVIEW);

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
	drawnTriangles = _world.display();
	glDisable(GL_CULL_FACE);
	if (showTriangleMesh)
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	if (showDebugInfo)
		debugBox.render();

	calculateFps();
	glfwSwapBuffers(_window);
}

void StoneEngine::updateChunks()
{
	chronoHelper.startChrono(0, "Load chunks");
	_world.loadChunks(camera.getWorldPosition());
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
	

	if (keyStates[GLFW_KEY_KP_ADD]) {_world.increaseRenderDistance(); _world.loadChunks(camera.getWorldPosition());}
	if (keyStates[GLFW_KEY_KP_SUBTRACT]) {_world.decreaseRenderDistance(); _world.loadChunks(camera.getWorldPosition());}

	if (!isWSL())
		rotationSpeed = (ROTATION_SPEED - 1.5) * deltaTime;
	else
		rotationSpeed = ROTATION_SPEED * deltaTime;

	start = std::chrono::steady_clock::now();
}

void StoneEngine::updateMovement()
{
	// Camera movement
	if (keyStates[GLFW_KEY_W]) camera.move(moveSpeed, 0.0, 0.0);
	if (keyStates[GLFW_KEY_A]) camera.move(0.0, moveSpeed, 0.0);
	if (keyStates[GLFW_KEY_S]) camera.move(-moveSpeed, 0.0, 0.0);
	if (keyStates[GLFW_KEY_D]) camera.move(0.0, -moveSpeed, 0.0);
	if (keyStates[GLFW_KEY_SPACE]) camera.move(0.0, 0.0, -moveSpeed);
	if (keyStates[GLFW_KEY_LEFT_SHIFT]) camera.move(0.0, 0.0, moveSpeed);

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

// void StoneEngine::chunkUpdateWorker()
// {
// 	while (running.load())
// 	{
// 		std::unique_lock<std::mutex> lock(chunksMutex);
// 		chunkCondition.wait(lock, [this] {return updateChunkFlag.load() || !running.load();});
// 		if (!running.load()) break ;

// 		updateChunkFlag.store(false);
// 		lock.unlock();
// 		updateChunks();
// 		usleep(2000);
// 	}
// }


void StoneEngine::updateChunkWorker()
{
	bool firstIteration = true;
	vec3 oldPos = camera.getWorldPosition();

	vec3 oldCamChunk = vec3(oldPos.x / CHUNK_SIZE, oldPos.y / CHUNK_SIZE, oldPos.z / CHUNK_SIZE);
	if (oldCamChunk.x < 0) oldCamChunk.x--;
	if (oldCamChunk.y < 0) oldCamChunk.y--;
	if (oldCamChunk.z < 0) oldCamChunk.z--;
	while (getIsRunning())
	{
		vec3 cameraPos = camera.getWorldPosition();
		if (oldPos.x != cameraPos.x || oldPos.y != cameraPos.y || oldPos.z != cameraPos.z || firstIteration)
		{
			// Check new chunk position for necessary updates to chunks
			vec3 camChunk(cameraPos.x / CHUNK_SIZE, cameraPos.y / CHUNK_SIZE, cameraPos.z / CHUNK_SIZE);
			if (cameraPos.x < 0) camChunk.x--;
			if (cameraPos.y < 0) camChunk.y--;
			if (cameraPos.z < 0) camChunk.z--;
			if (firstIteration || (updateChunk && (floor(oldCamChunk.x) != floor(camChunk.x) || floor(oldCamChunk.z) != floor(camChunk.z))))
			{
				updateChunks();
				
				firstIteration = false;
			}

			oldPos = cameraPos;
			oldCamChunk = vec3(oldPos.x / CHUNK_SIZE, oldPos.y / CHUNK_SIZE, oldPos.z / CHUNK_SIZE);
			if (oldCamChunk.x < 0) oldCamChunk.x--;
			if (oldCamChunk.y < 0) oldCamChunk.y--;
			if (oldCamChunk.z < 0) oldCamChunk.z--;
		}
	}
}

void StoneEngine::update()
{
	// Check for delta and apply to move and rotation speeds
	findMoveRotationSpeed();

	// Update player position and orientation
	updateMovement();

	// Update display
	display();

	// Register end of frame for the next delta
	end = std::chrono::steady_clock::now(); 
	delta = std::chrono::duration_cast<std::chrono::milliseconds>(start - end);
}

void StoneEngine::reshapeAction(int width, int height)
{
	glViewport(0, 0, width, height);
	glMatrixMode(GL_PROJECTION);

	projectionMatrix = glm::perspective(glm::radians(80.0f), (float)width / (float)height, 0.1f, 10000000.0f);
	glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projectionMatrix));
	//glLoadMatrixf(glm::value_ptr(projectionMatrix));
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