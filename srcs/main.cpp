#include "ft_vox.hpp"
#include "Chunk.hpp"
#include "Camera.hpp"
#include "globals.hpp"
#include "NoiseGenerator.hpp"

mat4 projectionMatrix;
mat4 viewMatrix;
bool keyStates[256] = {false};
bool specialKeyStates[256] = {false};
bool ignoreMouseEvent = false;

// FPS counter
int frameCount = 0;
double lastFrameTime = 0.0;
double currentFrameTime = 0.0;

//World gen
std::vector<ABlock> blocks;
std::vector<Chunk> chunks;
NoiseGenerator noise_gen(42);

void calculateFps()
{
	double fps = 0.0;
	frameCount++;
	currentFrameTime = glutGet(GLUT_ELAPSED_TIME);

	double timeInterval = currentFrameTime - lastFrameTime;

	if (timeInterval > 1000)
	{
		fps = frameCount / (timeInterval / 1000.0);

		lastFrameTime = currentFrameTime;
		frameCount = 0;

		std::stringstream title;
		title << "Not ft_minecraft | FPS: " << fps;
		glutSetWindowTitle(title.str().c_str());
	}
}

bool isWSL() {
	return (std::getenv("WSL_DISTRO_NAME") != nullptr); // WSL_DISTRO_NAME is set in WSL
}

void specialKeyPress(int key, int x, int y)
{
	(void)x;
	(void)y;
	specialKeyStates[key] = true;
}

void specialKeyRelease(int key, int x, int y)
{
	(void)x;
	(void)y;
	specialKeyStates[key] = false;
}

void closeCallback()
{
	//Closing window callback to free all data
	for (Chunk &chunk : chunks)
	{
		chunk.freeChunkData();
	}
	exit(0);
}

void keyPress(unsigned char key, int x, int y)
{
	(void)x;
	(void)y;
	keyStates[key] = true;
	if (key == 27)
		glutLeaveMainLoop();
}
void keyRelease(unsigned char key, int x, int y)
{
	(void)x;
	(void)y;
	keyStates[key] = false;
}

void mouseCallback(int x, int y)
{
	static bool firstMouse = true;
	static int lastX = 0, lastY = 0;

	// Get the current window size dynamically
	int windowWidth = glutGet(GLUT_WINDOW_WIDTH);
	int windowHeight = glutGet(GLUT_WINDOW_HEIGHT);

	// Update the size
	//particle_sys.update_window_size(windowWidth, windowHeight);
	cam.updateMousePos(x, y);


	int windowCenterX = windowWidth / 2;
	int windowCenterY = windowHeight / 2;

	if (firstMouse || ignoreMouseEvent) {
		lastX = windowCenterX;
		lastY = windowCenterY;
		firstMouse = false;
		ignoreMouseEvent = false;
		return ;
	}


	// Calculate the mouse movement offsets
	float xOffset = lastX - x;
	float yOffset = lastY - y; // Invert y-axis to match typical camera movement

	// Save the current mouse position for the next callback
	lastX = x;
	lastY = y;

	// Apply sensitivity to smooth the movement
	float sensitivity = 0.05f;
	xOffset *= sensitivity;
	yOffset *= sensitivity;

	// Update the camera angles
	cam.xangle += xOffset * cam.rotationspeed;
	cam.yangle += yOffset * cam.rotationspeed;

	// Limit the pitch (yangle) to avoid flipping
	if (cam.yangle > 89.0f) cam.yangle = 89.0f;
	if (cam.yangle < -89.0f) cam.yangle = -89.0f;

	// Ignore movement for next call to move mouse from warpPointer function
	ignoreMouseEvent = true;

	// Optionally, recenter the mouse after each movement
	glutWarpPointer(windowCenterX, windowCenterY);
}

void display()
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glMatrixMode(GL_MODELVIEW);

	// Matrix operations for the camera
	float radY;
	float radX;

	// Radians conversion
	radX = cam.xangle * (M_PI / 180.0);
	radY = cam.yangle * (M_PI / 180.0);

	viewMatrix = glm::mat4(1.0f);
	viewMatrix = glm::rotate(viewMatrix, radY, glm::vec3(-1.0f, 0.0f, 0.0f));
	viewMatrix = glm::rotate(viewMatrix, radX, glm::vec3(0.0f, -1.0f, 0.0f));
	viewMatrix = glm::translate(viewMatrix, glm::vec3(cam.position.x, cam.position.y, cam.position.z));
	glLoadMatrixf(glm::value_ptr(viewMatrix));
	
	textManager.displayAllTexture();

	glutSwapBuffers();
	calculateFps();
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

void update(int value)
{
	(void)value;
	vec3 oldCamChunk(-cam.position.x / 16, 0, -cam.position.z / 16);
	if (oldCamChunk.x < 0)
		oldCamChunk.x--;
	if (oldCamChunk.z < 0)
		oldCamChunk.z--;
	// Camera movement
	if (keyStates['z'] || keyStates['w']) cam.move(1.0, 0.0, 0.0);
	if (keyStates['q'] || keyStates['a']) cam.move(0.0, 1.0, 0.0);
	if (keyStates['s']) cam.move(-1.0, 0.0, 0.0);
	if (keyStates['d']) cam.move(0.0, -1.0, 0.0);
	if (keyStates[' ']) cam.move(0.0, 0.0, -1.0);
	if (keyStates['v']) cam.move(0.0, 0.0, 1.0);

	vec3 camChunk(-cam.position.x / 16, 0, -cam.position.z / 16);

	if (camChunk.x < 0)
		camChunk.x--;
	if (camChunk.z < 0)
		camChunk.z--;

	if (floor(oldCamChunk.x) != floor(camChunk.x) || floor(oldCamChunk.z) != floor(camChunk.z))
		updateChunks(camChunk);

	//Camera movement with keys
	if (specialKeyStates[GLUT_KEY_UP] && cam.yangle < 86.0) cam.yangle += cam.rotationspeed;
	if (specialKeyStates[GLUT_KEY_DOWN] && cam.yangle > -86.0) cam.yangle -= cam.rotationspeed;
	if (specialKeyStates[GLUT_KEY_RIGHT]) cam.xangle -= cam.rotationspeed;
	if (specialKeyStates[GLUT_KEY_LEFT]) cam.xangle += cam.rotationspeed;

	// Camera rotations
	if (cam.xangle > 360.0)
		cam.xangle = 0.0;
	else if (cam.xangle < 0.0)
		cam.xangle = 360.0;


	glutPostRedisplay();
	// Call update every 8 milliseconds (~120 FPS)
	glutTimerFunc(8, update, 0);
}

void reshape(int width, int height)
{
	glViewport(0, 0, width, height);

	// Apply projection matrix operations
	glMatrixMode(GL_PROJECTION);

	// Load identity matrix
	projectionMatrix = glm::mat4(1.0f);
	projectionMatrix = glm::perspective(glm::radians(45.0f), (float)width / (float)height, 1.0f, 1000.0f);
	glLoadMatrixf(glm::value_ptr(projectionMatrix));
}

void initGlutWindow(int ac, char **av)
{
	glutInit(&ac, av);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
	glutInitWindowSize(W_WIDTH, W_HEIGHT);
	glutCreateWindow("Not_ft_minecraft | FPS: 0");
	glEnable(GL_DEPTH_TEST);
	glutSetCursor(GLUT_CURSOR_NONE);
	glClearColor(0.1f, 0.1f, 0.1f, 1.0f); // Black background
}

void initGlutEvents()
{
	glutDisplayFunc(display);
	glutReshapeFunc(reshape);
	glutTimerFunc(8, update, 0); // 8 ticks per second update, 120 fps~
	glutKeyboardFunc(keyPress);
	glutKeyboardUpFunc(keyRelease);
	glutSpecialFunc(specialKeyPress);
	glutSpecialUpFunc(specialKeyRelease);
	if (isWSL() == false)
		glutPassiveMotionFunc(mouseCallback);
	glutCloseFunc(closeCallback);
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
	noise_gen.setSeed((size_t)seed);
	initGlutWindow(argc, argv);
	initGlutEvents();
	initGLEW();
	glEnable(GL_TEXTURE_2D);
	textManager.loadTexture(T_COBBLE, "textures/cobble.ppm");
	textManager.loadTexture(T_DIRT, "textures/dirt.ppm");
	textManager.loadTexture(T_GRASS_TOP, "textures/moss_block_.ppm");
	textManager.loadTexture(T_GRASS_SIDE, "textures/grass_block_side.ppm");
	textManager.loadTexture(T_STONE, "textures/stone.ppm");

	updateChunks(vec3(0, 0, 0));
	// Load textures
	glutMainLoop();
	return 0;
}
