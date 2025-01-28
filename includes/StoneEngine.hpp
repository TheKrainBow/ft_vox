#pragma once

#include "Chunk.hpp"
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

		mat4 projectionMatrix;
		mat4 viewMatrix;

		// Keys states and runtime booleans
		bool keyStates[348];
		bool ignoreMouseEvent;
		bool updateChunk;
		bool showTriangleMesh;
		bool mouseCaptureToggle;
		bool showDebugInfo;

		int windowHeight;
		int windowWidth;

		float moveSpeed;
		float rotationSpeed;

		// FPS counter
		int frameCount;
		double lastFrameTime;
		double currentFrameTime;
		double fps;

		// Debug data
		Chrono chronoHelper;
		double drawnTriangles;
	
		//World gen
		NoiseGenerator noise_gen;

		//Game time globals
		std::chrono::steady_clock::time_point start;
		std::chrono::steady_clock::time_point end;
		std::chrono::milliseconds delta;

		Textbox debugBox;
		TextureManager textureManager;
		Camera camera;
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
		void display(GLFWwindow* window);
		void updateChunks();

		// Movement methods
		void findMoveRotationSpeed();
		void update(GLFWwindow* window);
		void updateMovement();
};