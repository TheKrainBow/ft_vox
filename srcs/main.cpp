#include "ft_vox.hpp"

#include "blocks/Cobble.hpp"
#include "blocks/Dirt.hpp"
#include "Camera.hpp"

#include "globals.hpp"

mat4 projectionMatrix;
mat4 viewMatrix;
bool keyStates[256] = {false};
bool specialKeyStates[256] = {false};
Dirt *dirt;
Cobble *cobble;
std::vector<ABlock> blocks;
Camera cam;
bool ignoreMouseEvent = false;

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

	// Draw the dirt block
	for (std::vector<ABlock>::iterator it = blocks.begin(); it != blocks.end(); it++)
		it->display();

	glutSwapBuffers();
}

void update(int value)
{
	(void)value;
	// Camera movement
	if (keyStates['z'] || keyStates['w']) cam.move(1.0, 0.0, 0.0);
	if (keyStates['q'] || keyStates['a']) cam.move(0.0, 1.0, 0.0);
	if (keyStates['s']) cam.move(-1.0, 0.0, 0.0);
	if (keyStates['d']) cam.move(0.0, -1.0, 0.0);
	if (keyStates[' ']) cam.move(0.0, 0.0, -1.0);
	if (keyStates['v']) cam.move(0.0, 0.0, 1.0);

	//Camera movement with keys
	if (specialKeyStates[GLUT_KEY_UP]) cam.yangle += cam.rotationspeed;
	if (specialKeyStates[GLUT_KEY_DOWN]) cam.yangle -= cam.rotationspeed;
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
	glutCreateWindow("Not ft_minecraft");
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
}

int main(int argc, char **argv)
{
	int seed = 42;
	if (argc == 2)
	{
		seed = atoi(argv[1]);
		return 1;
	}


	initGlutWindow(argc, argv);
	initGlutEvents();
	textManager.addTexture(COBBLE, "textures/cobble.ppm");
	textManager.addTexture(DIRT, "textures/dirt.ppm");

	blocks.push_back(Cobble(0, 0, 0, 0));
	blocks.push_back(Cobble(1, 2, 0, 0));
	blocks.push_back(Dirt(2, 1, 0, 0));
	blocks.push_back(Dirt(1, 1, 0, 0));
	blocks.push_back(Dirt(1, 3, 0, 0));
	// Load textures
	std::cout << "This is a seed: " << seed << std::endl;
	glutMainLoop();
	return 0;
}
