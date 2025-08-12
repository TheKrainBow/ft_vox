#pragma once

#include "SubChunk.hpp"
#include "World.hpp"
#include "Camera.hpp"
#include "NoiseGenerator.hpp"
#include "Textbox.hpp"
#include "define.hpp"
#include "Chrono.hpp"

class StoneEngine {
	private:
		// Display
		GLFWwindow* _window;
		GLuint shaderProgram;
		GLuint sunProgram;
		Camera camera;
		World _world;
		int windowHeight;
		int windowWidth;
		mat4 projectionMatrix;
		mat4 viewMatrix;
		TextureManager _textureManager;
		ThreadPool &_pool;
		float _fov = 80.0f;

		// Framebuffer data
		GLuint fbo;
		GLuint fboTexture;
		GLuint dboTexture;
		GLuint rectangleVao;
		GLuint rectangleVbo;

		// Fbo shaders data
		GLuint fboShaderProgram;

		std::mutex		_isRunningMutex;
		bool			_isRunning = false;

		// Keys states and runtime booleans
		bool keyStates[348];
		bool ignoreMouseEvent;
		bool updateChunk;
		bool showTriangleMesh;
		bool mouseCaptureToggle;
		bool showDebugInfo;
		bool showLight;
		bool gravity;
		bool falling;

		// Player speed
		float moveSpeed;
		float rotationSpeed;
		float fallSpeed = 0.0f;

		// FPS counter
		int frameCount;
		double lastFrameTime;
		double currentFrameTime;
		double fps;

		// Debug
		Chrono chronoHelper;
		double drawnTriangles;
		Textbox debugBox;
	
		// World gen
		NoiseGenerator noise_gen;

		// Game time
		std::chrono::steady_clock::time_point start;
		std::chrono::steady_clock::time_point end;
		std::chrono::milliseconds delta;
		float deltaTime;

		// Game data
		ivec3 sunPosition;
		std::atomic_int timeValue;
	public:
		StoneEngine(int seed, ThreadPool &pool);
		~StoneEngine();
		void run();
	private:
		// Event hook actions
		void keyAction(int key, int scancode, int action, int mods);
		void mouseAction(double x, double y);
		void reshapeAction(int width, int height);
		void scrollAction(double yoffset);

		// Event hook callbacks
		static void reshape(GLFWwindow* window, int width, int height); 
		static void keyPress(GLFWwindow* window, int key, int scancode, int action, int mods);
		static void mouseCallback(GLFWwindow* window, double x, double y);
		static void scrollCallback(GLFWwindow* window, double xoffset, double yoffset);

		// Init methods
		void	initData();
		void	initGLEW();
		int		initGLFW();
		void	initTextures();
		void	initRenderShaders();
		void	initDebugTextBox();
		void	initFramebuffers();
		void	initFboShaders();
		void	resetFrameBuffers();
		void	updateFboWindowSize();

		// Runtime methods
		void calculateFps();
		void display();
		void loadFirstChunks();
		void loadNextChunks(ivec2 newCamChunk);
		void activateRenderShader();
		void activateFboShader();
		void triangleMeshToggle();
		ivec2 getChunkPos(ivec2 pos);
		bool canMove(const glm::vec3& offset, float extra);

		// Multi thread methods
		//void chunkUpdateWorker();

		// Movement methods
		void findMoveRotationSpeed();
		void update();
		void updateMovement();
		void updateGameTick();

		void updateChunkWorker();

		bool getIsRunning();
};