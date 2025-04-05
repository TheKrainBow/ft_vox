#pragma once

#include "SubChunk.hpp"
#include "World.hpp"
#include "Camera.hpp"
#include "NoiseGenerator.hpp"
#include "BiomeGenerator.hpp"
#include "Textbox.hpp"
#include "define.hpp"
#include "Chrono.hpp"

class StoneEngine {
	private:
		// Display
		GLFWwindow* _window;
		GLuint shaderProgram;
		GLuint sunProgram;
		World _world;
		int windowHeight;
		int windowWidth;
		Camera camera;
		mat4 projectionMatrix;
		glm::mat4 viewMatrix;
		TextureManager _textureManager;

		// Framebuffer data
		GLuint fbo;
		GLuint fboTexture;
		GLuint rectangleVao;
		GLuint rectangleVbo;

		// Render buffer data
		GLuint rbo;

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

		// Player speed
		float moveSpeed;
		float rotationSpeed;

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

		// Game data
		glm::vec3 sunPosition;
		std::atomic_int timeValue;
	public:
		StoneEngine(int seed);
		~StoneEngine();
		void run();
	private:
		// Event hook actions
		void keyAction(int key, int scancode, int action, int mods);
		void mouseAction(double x, double y);
		void reshapeAction(int width, int height);

		// Event hook callbacks
		static void reshape(GLFWwindow* window, int width, int height); 
		static void keyPress(GLFWwindow* window, int key, int scancode, int action, int mods);
		static void mouseCallback(GLFWwindow* window, double x, double y);

		// Init methods
		void	initData();
		void	initGLEW();
		int		initGLFW();
		void	initTextures();
		void	initShaders();
		void	initDebugTextBox();
		void	initFramebuffers();

		// Runtime methods
		void calculateFps();
		void display();
		void loadFirstChunks();
		void loadNextChunks(ivec2 newCamChunk);

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