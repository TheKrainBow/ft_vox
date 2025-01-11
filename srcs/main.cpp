#include "ft_vox.hpp"
#include "Chunk.hpp"
#include "World.hpp"
#include "Camera.hpp"
#include "globals.hpp"
#include "NoiseGenerator.hpp"

#include "Textbox.hpp"

// Display
GLFWwindow* _window;
GLuint shaderProgram;
World *_world;

mat4 projectionMatrix;
mat4 viewMatrix;
bool keyStates[348] = {false};
bool ignoreMouseEvent = false;
bool updateChunk = true;
bool showDebugInfo = true;
bool showTriangleMesh = false;

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
// std::vector<Chunk> chunks;
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

bool isWSL() {
	return (std::getenv("WSL_DISTRO_NAME") != nullptr); // WSL_DISTRO_NAME is set in WSL
}

void keyPress(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	(void)window;
	(void)scancode;
	(void)mods;

	if (action == GLFW_PRESS) keyStates[key] = true;
	else if (action == GLFW_RELEASE) keyStates[key] = false;
	if (action == GLFW_PRESS && key == GLFW_KEY_C) updateChunk = !updateChunk;
	if (action == GLFW_PRESS && key == GLFW_KEY_F3) showDebugInfo = !showDebugInfo;
	if (action == GLFW_PRESS && key == GLFW_KEY_F4) showTriangleMesh = !showTriangleMesh;
	if (key == GLFW_KEY_ESCAPE) glfwSetWindowShouldClose(_window, GL_TRUE);
}

GLuint compileShader(const char* filePath, GLenum shaderType)
{
    std::ifstream shaderFile(filePath);
    if (!shaderFile.is_open()) {
        std::cerr << "Error: Shader file could not be opened: " << filePath << std::endl;
        return 0;
    }

    std::stringstream shaderStream;
    shaderStream << shaderFile.rdbuf();
    std::string shaderCode = shaderStream.str();
    const char* shaderSource = shaderCode.c_str();

    GLuint shader = glCreateShader(shaderType);
    glShaderSource(shader, 1, &shaderSource, NULL);
    glCompileShader(shader);

    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, NULL, infoLog);
        std::cerr << "Error: Shader compilation failed\n" << infoLog << std::endl;
    }
    return shader;
}

GLuint createShaderProgram(const char* vertexShaderPath, const char* fragmentShaderPath)
{
    GLuint vertexShader = compileShader(vertexShaderPath, GL_VERTEX_SHADER);
    GLuint fragmentShader = compileShader(fragmentShaderPath, GL_FRAGMENT_SHADER);

    GLuint shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    GLint success;
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
        std::cerr << "Error: Shader program linking failed\n" << infoLog << std::endl;
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    return shaderProgram;
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

	while (cam.xangle < 0)
		cam.xangle += 360;
	while (cam.xangle >= 360)
		cam.xangle -= 360;

	ignoreMouseEvent = true;

	glfwSetCursorPos(_window, windowCenterX, windowCenterY);
}

void display(GLFWwindow* window)
{
	(void)window;
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glMatrixMode(GL_MODELVIEW);

	glm::mat4 viewMatrix = glm::lookAt(
		cam.position,         // cam position
		cam.center,           // Look-at point
		glm::vec3(0.0f, 1.0f, 0.0f) // Up direction
	);
	glm::mat4 modelMatrix = glm::mat4(1.0f);


	float radY, radX;
	radX = cam.xangle * (M_PI / 180.0);
	radY = cam.yangle * (M_PI / 180.0);

	viewMatrix = glm::mat4(1.0f);
	viewMatrix = glm::rotate(viewMatrix, radY, glm::vec3(-1.0f, 0.0f, 0.0f));
	viewMatrix = glm::rotate(viewMatrix, radX, glm::vec3(0.0f, -1.0f, 0.0f));
	viewMatrix = glm::translate(viewMatrix, glm::vec3(cam.position.x, cam.position.y, cam.position.z));

		
	glUseProgram(shaderProgram);
	glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projectionMatrix));
	glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(modelMatrix));
	glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(viewMatrix));
	
	#ifdef NDEBUG
	for (std::vector<Chunk>::iterator it = chunks.begin(); it != chunks.end(); it++)
	{
		it->renderBoundaries();
	}
	#endif
	
	if (showTriangleMesh)
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	else
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);      // Cull back faces
	glFrontFace(GL_CCW);      // Set counter-clockwise as the front face

	triangleDrown = textManager.displayAllTexture(cam);

	glDisable(GL_CULL_FACE);
	if (showTriangleMesh)
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	if (showDebugInfo)
		debugBox->render();

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

void updateChunks()
{
	_world->loadChunk(vec3(-cam.position.x, -cam.position.y, -cam.position.z), RENDER_DISTANCE);	
	textManager.resetTextureVertex();
	_world->sendFacesToDisplay();
	// for (std::vector<Chunk>::iterator it = chunks.begin(); it != chunks.end(); it++)
	// 	it->display();
	textManager.processTextureVertex();
}

void update(GLFWwindow* window)
{
	(void)window;

	vec3 oldCamChunk(-cam.position.x / 16, -cam.position.y / 16, -cam.position.z / 16);
	if (oldCamChunk.x < 0) oldCamChunk.x--;
	if (oldCamChunk.y < 0) oldCamChunk.y--;
	if (oldCamChunk.z < 0) oldCamChunk.z--;

	if (keyStates[GLFW_KEY_Z] || keyStates[GLFW_KEY_W]) cam.move(1.0, 0.0, 0.0);
	if (keyStates[GLFW_KEY_Q] || keyStates[GLFW_KEY_A]) cam.move(0.0, 1.0, 0.0);
	if (keyStates[GLFW_KEY_S]) cam.move(-1.0, 0.0, 0.0);
	if (keyStates[GLFW_KEY_D]) cam.move(0.0, -1.0, 0.0);
	if (keyStates[GLFW_KEY_SPACE]) cam.move(0.0, 0.0, -1.0);
	if (keyStates[GLFW_KEY_LEFT_SHIFT]) cam.move(0.0, 0.0, 1.0);

	vec3 camChunk(-cam.position.x / 16, -cam.position.y / 16, -cam.position.z / 16);
	if (camChunk.x < 0) camChunk.x--;
	if (camChunk.y < 0) camChunk.y--;
	if (camChunk.z < 0) camChunk.z--;

	if (updateChunk && (floor(oldCamChunk.x) != floor(camChunk.x) || floor(oldCamChunk.y) != floor(camChunk.y) || floor(oldCamChunk.z) != floor(camChunk.z)))
		updateChunks();

	if (keyStates[GLFW_KEY_UP] && cam.yangle < 90.0) cam.yangle += cam.rotationspeed;
	if (keyStates[GLFW_KEY_DOWN] && cam.yangle > -90.0) cam.yangle -= cam.rotationspeed;
	if (keyStates[GLFW_KEY_RIGHT]) cam.xangle -= cam.rotationspeed;
	if (keyStates[GLFW_KEY_LEFT]) cam.xangle += cam.rotationspeed;

	while (cam.xangle < 0)
		cam.xangle += 360;
	while (cam.xangle >= 360)
		cam.xangle -= 360;

	display(_window);
}

void reshape(GLFWwindow* window, int width, int height)
{
	(void)window;
	glViewport(0, 0, width, height);
	glMatrixMode(GL_PROJECTION);
	
	projectionMatrix = glm::perspective(glm::radians(45.0f), (float)width / (float)height, 1.0f, 1000.0f);
	glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projectionMatrix));
	//glLoadMatrixf(glm::value_ptr(projectionMatrix));
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
	textManager.loadTexture(T_GRASS_TOP, "textures/grass_block_top_colored.ppm");
	textManager.loadTexture(T_GRASS_SIDE, "textures/grass_block_side.ppm");
	textManager.loadTexture(T_STONE, "textures/stone.ppm");

	World overworld(42);

	_world = &overworld;
	// chunks.push_back(Chunk(-cam.position.x / 16 - 1, -cam.position.z / 16 - 1, noise_gen));
	updateChunks();

	reshape(_window, windowWidth, windowHeight);
	glEnable(GL_DEPTH_TEST);

	shaderProgram = createShaderProgram("shaders/basic.vert", "shaders/basic.frag");

	//// Set the active shader program
	glm::mat4 projectionMatrix = glm::perspective(glm::radians(45.0f), (float)W_WIDTH / (float)W_HEIGHT, 0.1f, 1000.0f);

	glUseProgram(shaderProgram);
	glUniform1i(glGetUniformLocation(shaderProgram, "texture1"), 0);  // Use texture unit 0
	glUniform1i(glGetUniformLocation(shaderProgram, "useTexture"), GL_FALSE);  // Use texture unit 0
	glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projectionMatrix));

	Textbox debugBoxObject(_window, 0, 0, 200, 200);
	debugBox = &debugBoxObject;
	debugBoxObject.loadFont("textures/CASCADIAMONO.TTF", 20);
	debugBoxObject.addLine("FPS: ", Textbox::DOUBLE, &fps);
	debugBoxObject.addLine("Triangles: ", Textbox::DOUBLE, &triangleDrown);
	debugBoxObject.addLine("x: ", Textbox::FLOAT, &cam.position.x);
	debugBoxObject.addLine("y: ", Textbox::FLOAT, &cam.position.y);
	debugBoxObject.addLine("z: ", Textbox::FLOAT, &cam.position.z);
	debugBoxObject.addLine("xangle: ", Textbox::FLOAT, &cam.xangle);
	debugBoxObject.addLine("yangle: ", Textbox::FLOAT, &cam.yangle);
	glClearColor(0.53f, 0.81f, 0.92f, 1.0f); // Soft sky blue
	// Main loop
	while (!glfwWindowShouldClose(_window))
	{
		//glClear(GL_COLOR_BUFFER_BIT);
		update(_window);
		glfwPollEvents();
	}

	//Free chunk data
	// for (Chunk &chunk : chunks)
	// {
	// 	chunk.freeChunkData();
	// }

	glfwDestroyWindow(_window);
	glfwTerminate();
	return 0;
}
