#include "ft_vox.hpp"
#include "Chunk.hpp"
#include "Camera.hpp"
#include "globals.hpp"
#include "NoiseGenerator.hpp"

#include "Textbox.hpp"

// Display
GLFWwindow* _window;

mat4 projectionMatrix;
mat4 viewMatrix;
bool keyStates[348] = {false};
bool ignoreMouseEvent = false;
bool updateChunk = true;
int windowHeight = W_HEIGHT;
int windowWidth = W_WIDTH;

// FPS counter
int frameCount = 0;
double lastFrameTime = 0.0;
double currentFrameTime = 0.0;
double fps = 0.0;
double triangleDrown = 0.0;

//World gen
std::vector<ABlock> blocks;
std::vector<Chunk> chunks;
NoiseGenerator noise_gen(42);

Textbox *debugBox;

void calculateFps()
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

void reshape(GLFWwindow* window, int width, int height)
{
	(void)window;
	windowWidth = width;
	windowHeight = height;
	glViewport(0, 0, width, height);
	glMatrixMode(GL_PROJECTION);
	projectionMatrix = glm::mat4(1.0f);
	projectionMatrix = glm::perspective(glm::radians(45.0f), (float)width / (float)height, 1.0f, 1000.0f);
	glLoadMatrixf(glm::value_ptr(projectionMatrix));
}

bool isWSL() {
	return (std::getenv("WSL_DISTRO_NAME") != nullptr); // WSL_DISTRO_NAME is set in WSL
}

void keyPress(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	(void)window;
	(void)scancode;
	(void)mods;
	if (action == GLFW_PRESS)
		keyStates[key] = true;
	else if (action == GLFW_RELEASE)
		keyStates[key] = false;
	if (action == GLFW_PRESS && key == GLFW_KEY_C)
		updateChunk = !updateChunk;
	if (key == GLFW_KEY_ESCAPE)
		glfwSetWindowShouldClose(_window, GL_TRUE);
}

void mouseCallback(GLFWwindow* window, double x, double y)
{
	(void)window;
	static bool firstMouse = true;
	static double lastX = 0, lastY = 0;

	// Get the current window size dynamically
	int windowWidth, windowHeight;
	glfwGetWindowSize(_window, &windowWidth, &windowHeight);
	glfwSetInputMode(_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

	cam.updateMousePos(x, y);

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

	cam.xangle += xOffset * cam.rotationspeed;
	cam.yangle += yOffset * cam.rotationspeed;

	if (cam.yangle > 89.0f) cam.yangle = 89.0f;
	if (cam.yangle < -89.0f) cam.yangle = -89.0f;

	ignoreMouseEvent = true;

	glfwSetCursorPos(_window, windowCenterX, windowCenterY);
}

void display(GLFWwindow* window)
{
	(void)window;
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glMatrixMode(GL_MODELVIEW);

	float radY, radX;
	radX = cam.xangle * (M_PI / 180.0);
	radY = cam.yangle * (M_PI / 180.0);

	viewMatrix = glm::mat4(1.0f);
	viewMatrix = glm::rotate(viewMatrix, radY, glm::vec3(-1.0f, 0.0f, 0.0f));
	viewMatrix = glm::rotate(viewMatrix, radX, glm::vec3(0.0f, -1.0f, 0.0f));
	viewMatrix = glm::translate(viewMatrix, glm::vec3(cam.position.x, cam.position.y, cam.position.z));
	glLoadMatrixf(glm::value_ptr(viewMatrix));
	#ifdef NDEBUG
	for (std::vector<Chunk>::iterator it = chunks.begin(); it != chunks.end(); it++)
	{
		it->renderBoundaries();
	}
	#endif
	triangleDrown = textManager.displayAllTexture();

	(void)window;
	debugBox->render();
	//reshape(_window, windowWidth, windowHeight);
	calculateFps();
	glfwSwapBuffers(_window);
}

struct pair_hash {
	template <class T1, class T2>
	std::size_t operator () (const std::pair<T1, T2>& pair) const {
		auto h1 = std::hash<T1>{}(pair.first);
		auto h2 = std::hash<T2>{}(pair.second);
		return h1 ^ (h2 << 1);
	}
};

void updateChunks(vec3 newCameraPosition)
{
	// Store the set of positions for currently loaded chunks
	std::unordered_set<std::pair<int, int>, pair_hash> loadedChunkPositions;
	for (Chunk& chunk : chunks)
		loadedChunkPositions.emplace(chunk.getPosition().x, chunk.getPosition().y);

	// Set to track positions of chunks that should remain
	std::unordered_set<std::pair<int, int>, pair_hash> requiredChunkPositions;

	// Add chunks within the render distance
	for (int x = -RENDER_DISTANCE / 2; x < RENDER_DISTANCE / 2; x++)
	{
		for (int z = -RENDER_DISTANCE / 2; z < RENDER_DISTANCE / 2; z++)
		{
			int chunkX = newCameraPosition.x + x;
			int chunkZ = newCameraPosition.z + z;

			// Add this position to the required set
			requiredChunkPositions.emplace(chunkX, chunkZ);

			// If this chunk is not already loaded, create and add it
			if (loadedChunkPositions.find({chunkX, chunkZ}) == loadedChunkPositions.end())
			{
				// std::cout << "Add chunk (" << chunkX << ", " << chunkZ << ")" << std::endl;
				chunks.push_back(Chunk(chunkX, chunkZ, noise_gen));
			}
		}
	}

	// Remove chunks that are no longer needed
	chunks.erase(
		std::remove_if(chunks.begin(), chunks.end(),
			[&requiredChunkPositions](Chunk& chunk)
			{
				if (requiredChunkPositions.find({chunk.getPosition().x, chunk.getPosition().y}) == requiredChunkPositions.end())
				{
					chunk.freeChunkData();
					return true;
				}
				return false;
			}),
		chunks.end()
	);
	
	textManager.resetAllTextureVertex();

	for (std::vector<Chunk>::iterator it = chunks.begin(); it != chunks.end(); it++)
		it->display();
}

void update(GLFWwindow* window)
{
	(void)window;
	vec3 oldCamChunk(-cam.position.x / 16, 0, -cam.position.z / 16);
	if (oldCamChunk.x < 0) oldCamChunk.x--;
	if (oldCamChunk.z < 0) oldCamChunk.z--;

	if (keyStates[GLFW_KEY_Z] || keyStates[GLFW_KEY_W]) cam.move(1.0, 0.0, 0.0);
	if (keyStates[GLFW_KEY_Q] || keyStates[GLFW_KEY_A]) cam.move(0.0, 1.0, 0.0);
	if (keyStates[GLFW_KEY_S]) cam.move(-1.0, 0.0, 0.0);
	if (keyStates[GLFW_KEY_D]) cam.move(0.0, -1.0, 0.0);
	if (keyStates[GLFW_KEY_SPACE]) cam.move(0.0, 0.0, -1.0);
	if (keyStates[GLFW_KEY_LEFT_SHIFT]) cam.move(0.0, 0.0, 1.0);

	vec3 camChunk(-cam.position.x / 16, 0, -cam.position.z / 16);
	if (camChunk.x < 0) camChunk.x--;
	if (camChunk.z < 0) camChunk.z--;

	if (updateChunk && (floor(oldCamChunk.x) != floor(camChunk.x) || floor(oldCamChunk.z) != floor(camChunk.z)))
		updateChunks(camChunk);

	if (keyStates[GLFW_KEY_UP] && cam.yangle < 86.0) cam.yangle += cam.rotationspeed;
	if (keyStates[GLFW_KEY_DOWN] && cam.yangle > -86.0) cam.yangle -= cam.rotationspeed;
	if (keyStates[GLFW_KEY_RIGHT]) cam.xangle -= cam.rotationspeed;
	if (keyStates[GLFW_KEY_LEFT]) cam.xangle += cam.rotationspeed;

	if (cam.xangle > 360.0) cam.xangle = 0.0;
	else if (cam.xangle < 0.0) cam.xangle = 360.0;

	display(_window);
}

int initGLFW()
{
	_window = glfwCreateWindow(windowWidth, windowHeight, "Not_ft_minecraft | FPS: 0", NULL, NULL);
	if (!_window)
	{
		std::cerr << "Failed to create GLFW window" << std::endl;
		glfwTerminate();
		return 0;
	}

	glfwMakeContextCurrent(_window);
	glfwSetFramebufferSizeCallback(_window, reshape);
	glfwSetKeyCallback(_window, keyPress);
	if (!isWSL())
		glfwSetCursorPosCallback(_window, mouseCallback);
		
	return 1;
}

void initGLEW() {
	GLenum err = glewInit();
	if (err != GLEW_OK)
	{
		std::cerr << "GLEW initialization failed: " << glewGetErrorString(err) << std::endl;
		return ;
	}
}

int main(int argc, char **argv)
{
	int seed = 42;
	if (argc == 2)
	{
		seed = atoi(argv[1]);
		return 1;
	}
	if (!glfwInit())
	{
		std::cerr << "Failed to initialize GLFW" << std::endl;
		return -1;
	}

	noise_gen.setSeed((size_t)seed);
	if (!initGLFW())
		return 1;

	initGLEW();
	glEnable(GL_TEXTURE_2D);
	textManager.loadTexture(T_COBBLE, "textures/cobble.ppm");
	textManager.loadTexture(T_DIRT, "textures/dirt.ppm");
	textManager.loadTexture(T_GRASS_TOP, "textures/moss_block_.ppm");
	textManager.loadTexture(T_GRASS_SIDE, "textures/grass_block_side.ppm");
	textManager.loadTexture(T_STONE, "textures/stone.ppm");

	updateChunks(vec3(0, 0, 0));

	reshape(_window, windowWidth, windowHeight);
	glEnable(GL_DEPTH_TEST);

	Textbox debugBoxObject(_window, 0, 0, 200, 200);
	debugBox = &debugBoxObject;
	debugBoxObject.loadFont("textures/CASCADIAMONO.TTF", 20);
	debugBoxObject.addLine("FPS: ", &fps);
	debugBoxObject.addLine("Triangles: ", &triangleDrown);
	// Main loop
	while (!glfwWindowShouldClose(_window))
	{
		//glClear(GL_COLOR_BUFFER_BIT);
		update(_window);
		glfwPollEvents();
	}
	//Free chunk data
	for (Chunk &chunk : chunks)
	{
		chunk.freeChunkData();
	}

	glfwDestroyWindow(_window);
	glfwTerminate();
	return 0;
}
