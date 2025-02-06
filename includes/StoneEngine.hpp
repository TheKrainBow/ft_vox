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
		World *_world;
		int windowHeight;
		int windowWidth;
		Camera camera;
		mat4 projectionMatrix;
		TextureManager textureManager;

		bool _isRunning = false;

		// Keys states and runtime booleans
		bool keyStates[348];
		bool ignoreMouseEvent;
		bool updateChunk;
		bool showTriangleMesh;
		bool mouseCaptureToggle;
		bool showDebugInfo;

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
	
		// Multi-Threads data
		// std::mutex chunksMutex;
		// std::mutex printMutex;

		// std::thread chunkUpdateThread;
		// std::thread displayThread;
		// std::atomic<bool> updateChunkFlag;
		// std::atomic<bool> running;
		// std::condition_variable chunkCondition;

		// Game time
		std::chrono::steady_clock::time_point start;
		std::chrono::steady_clock::time_point end;
		std::chrono::milliseconds delta;
	public:
		StoneEngine(World *world);
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

		// Runtime methods
		void calculateFps();
		void display();
		void updateChunks();

		// Multi thread methods
		//void chunkUpdateWorker();

		// Movement methods
		void findMoveRotationSpeed();
		void update(GLFWwindow* window);
		void updateMovement();

		void updateChunkWorker();
};