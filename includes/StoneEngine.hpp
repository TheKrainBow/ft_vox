#pragma once

#include <cstddef>

#include "SubChunk.hpp"
#include "World.hpp"
#include "Camera.hpp"
#include "NoiseGenerator.hpp"
#include "Textbox.hpp"
#include "define.hpp"
#include "Chrono.hpp"
#include "Skybox.hpp"

class StoneEngine {
	public:

	enum GridDebugMode { GRID_OFF=0, GRID_CHUNKS=1, GRID_SUBCHUNKS=2, GRID_BOTH=3 };
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
			FOG = 1,
			GODRAYS = 2,
			CROSSHAIR = 3,
			SKYBOX_COMPOSITE = 4,
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

		// Skybox
		GLuint skyboxProgram = 0;
		Skybox _skybox;
		bool _hasSkybox = false;

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
		std::atomic_bool	_isRunning = false;

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
		bool ascending;
		bool sprinting;
		GridDebugMode _gridMode = GRID_OFF;

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
		int drawnTriangles;
		Textbox debugBox;
		size_t _processMemoryUsage = 0;
	
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
		std::chrono::steady_clock::time_point _placeCooldown;
		std::chrono::steady_clock::time_point now;
		TopBlock camTopBlock;
		movedir playerDir;
		int _bestRender = 0;
		int	_biome;
		double _humidity;
		double _temperature;
		GLuint _wireVAO = 0, _wireVBO = 0;
		GLuint _wireProgram = 0;

		// Selected block for placement
		BlockType	selectedBlock;
		block_types	selectedBlockDebug;
		bool placing;
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
		void mouseButtonAction(int button, int action, int mods);
		void postProcessCrosshair();
		

		// Event hook callbacks
		static void reshape(GLFWwindow* window, int width, int height); 
		static void keyPress(GLFWwindow* window, int key, int scancode, int action, int mods);
		static void mouseCallback(GLFWwindow* window, double x, double y);
		static void scrollCallback(GLFWwindow* window, double xoffset, double yoffset);
		static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);

		// Init methods
		void	initData();
		void	initGLEW();
		int		initGLFW();
		void	initTextures();
		void	initRenderShaders();
		void	initSkybox();
		void	initDebugTextBox();
		void	initFramebuffers(FBODatas &pingFBO, int w, int h);
		void	initFboShaders();
		void	resetFrameBuffers();
		void	updateFboWindowSize(PostProcessShader &shader);
		void	initMsaaFramebuffers(FBODatas &fboData, int width, int height);
		void   initWireframeResources();
		void   renderAimHighlight();
		void   postProcessSkyboxComposite();

		// Runtime methods
		void calculateFps();
		void display();
		void renderSkybox();
		void renderOverlayAndUI();
		void finalizeFrame();
		void renderTransparentObjects();
		void renderSolidObjects();
		void resolveMsaaToFbo();
		void prepareRenderPipeline();
		void displaySun();
		void loadFirstChunks();
		void loadNextChunks(ivec2 newCamChunk);
		void activateRenderShader();
		void renderPlanarReflection();
		ivec2 getChunkPos(vec2 camPosXZ);
		bool canMove(const glm::vec3& offset, float extra);
		void updatePlayerStates();
		void updateFalling(vec3 &worldPos, int &blockHeight);
		void updateSwimming(BlockType block);
		void updateJumping();
		void updatePlayerDirection();
		void updateProcessMemoryUsage();
		bool tryMoveStepwise(const glm::vec3& moveVec, float stepSize);
		void activateTransparentShader();
		void updateBiomeData();
		void renderChunkGrid();
	
		void screenshotFBOBuffer(FBODatas &source, FBODatas &destination);
		void postProcessGreedyFix();
		void postProcessFog();
		void postProcessGodRays();
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
		void updatePlacing();
		void updateChunkWorker();

		bool getIsRunning();
};
