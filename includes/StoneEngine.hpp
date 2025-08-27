#pragma once

#include "SubChunk.hpp"
#include "World.hpp"
#include "Camera.hpp"
#include "NoiseGenerator.hpp"
#include "Textbox.hpp"
#include "define.hpp"
#include "Chrono.hpp"

class StoneEngine {
	public:
	typedef struct s_PostProcessShader {
		GLuint vao;
		GLuint vbo;
		GLuint program;
	} PostProcessShader;

	typedef struct s_FBODatas {
		GLuint fbo;
		GLuint texture;
		GLuint depth;
	} FBODatas;

	typedef enum {
		GREEDYFIX = 0,
		GODRAYS = 1,
		FOG = 2,
		BRIGHNESSMASK = 3,
		GODRAYS_BLEND = 4,
	} ShaderType;
	private:
		// Display
		GLFWwindow* _window;
		GLint _maxSamples;
		GLuint shaderProgram;
		GLuint sunProgram;
		Camera camera;

		GLuint sunShaderProgram;
		GLuint sunVAO;
		GLuint sunVBO;

		GLuint waterShaderProgram;
		GLuint waterNormalMap;

		std::map<ShaderType, PostProcessShader> postProcessShaders;
		
		FBODatas msaaFBO;
		FBODatas readFBO;
		FBODatas writeFBO;
		FBODatas tmpFBO;

		World _world;
		int windowHeight;
		int windowWidth;
		mat4 projectionMatrix;
		mat4 viewMatrix;
		TextureManager _textureManager;
		ThreadPool &_pool;
		float _fov = 80.0f;

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
		bool swimming;
		bool jumping;
		bool isUnderWater;

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
		std::chrono::steady_clock::time_point _jumpCooldown;
		std::chrono::steady_clock::time_point _swimUpCooldownOnRise;
		std::chrono::steady_clock::time_point now;
		TopBlock camTopBlock;
		movedir playerDir;
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
		void	initFramebuffers(FBODatas &pingFBO, int w, int h);
		void	initFboShaders();
		void	resetFrameBuffers();
		void	updateFboWindowSize(PostProcessShader &shader);
		void	initMsaaFramebuffers(FBODatas &fboData, int width, int height);

		// Runtime methods
		void calculateFps();
		void display();
		void renderOverlayAndUI();
		void finalizeFrame();
		void renderTransparentObjects();
		void renderSceneToFBO();
		void resolveMsaaToFbo();
		void prepareRenderPipeline();
		void displaySun();
		void loadFirstChunks();
		void loadNextChunks(ivec2 newCamChunk);
		void activateRenderShader();
		void activateFboShader();
		void triangleMeshToggle();
		ivec2 getChunkPos(ivec2 pos);
		bool canMove(const glm::vec3& offset, float extra);
		void updatePlayerStates();
		void updateFalling(vec3 worldPos, int blockHeight);
		void updateSwimming(BlockType block);
		void updateJumping();
		void updatePlayerDirection();
		bool tryMoveStepwise(const glm::vec3& moveVec, float stepSize);
		void activateTransparentShader();
	
		void screenshotFBOBuffer(FBODatas &source, FBODatas &destination);
		void postProcessGreedyFix();
		void postProcessBrightnessMask();
		void postProcessFog();
		void postProcessGodRays();
		void postProcessGodRaysBlend();
		void sendPostProcessFBOToDispay();
	
		PostProcessShader createPostProcessShader(PostProcessShader &shader, const std::string& vertPath, const std::string& fragPath);

		vec3 computeSunPosition(int timeValue, const glm::vec3& cameraPos);

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