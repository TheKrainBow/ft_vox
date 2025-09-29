#pragma once

#include "SubChunk.hpp"
#include "Camera.hpp"
#include "NoiseGenerator.hpp"
#include "Textbox.hpp"
#include "define.hpp"
#include "Chrono.hpp"
#include "Skybox.hpp"
#include "ChunkManager.hpp"
#include "Raycaster.hpp"
#include <cstddef>
#include <string>

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

		// Flowers (cutout pass)
		GLuint flowerProgram = 0;
		GLuint flowerVAO = 0;
		GLuint flowerVBO = 0;          // base X-quad mesh
		GLuint flowerInstanceVBO = 0;  // per-instance data
		GLuint flowerTexture = 0;      // 2D array of flower sprites
		GLsizei flowerInstanceCount = 0;
		int flowerLayerCount = 0;
		int flowerShortGrassLayer = -1; // texture array layer for short_grass, if present
		int _layerPoppy = -1;
		int _layerDandelion = -1;
		int _layerCyan = -1;
		struct FlowerInstance { glm::vec3 pos; float rot; float scale; float heightScale; int typeId; };
		// Plants stored per chunk/subchunk for visibility lifetime
		std::unordered_map<glm::ivec2, std::unordered_map<int, std::vector<FlowerInstance>>, ivec2_hash> _flowersBySub;
		// Rebuilt each frame for visible chunks
		std::vector<FlowerInstance> _visibleFlowers;

		std::map<ShaderType, PostProcessShader> postProcessShaders;
		
		FBODatas msaaFBO;
		FBODatas readFBO;
		FBODatas writeFBO;
		FBODatas tmpFBO;

		int windowHeight;
		int windowWidth;
		bool _isFullscreen = true;
		int  _windowedX = 100;
		int  _windowedY = 100;
		int  _windowedW = W_WIDTH;
		int  _windowedH = W_HEIGHT;

		// Loading screen
		Textbox _loadingBox;
		bool _loadingInit = false;
		std::string _loadingText = "Loading...";
		std::chrono::steady_clock::time_point _splashDeadline;
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
		bool showHelp;
		bool showUI;
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

		// Debug / Overlays
		Chrono chronoHelper;
		int drawnTriangles;
		Textbox debugBox;
		Textbox helpBox;
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
		ChunkManager _chunkMgr;

		// Occlusion control: disable previous-frame occlusion for a few frames
		// after edits to avoid one-frame popping when geometry changes.
		int _occlDisableFrames = 0;

		// Help overlay dynamic status strings
		std::string _hGravity;
		std::string _hGeneration;
		std::string _hSprinting;
		std::string _hUI;
		std::string _hLighting;
		std::string _hMouseCapture;
		std::string _hDebug;
		std::string _hHelp;
		std::string _hWireframe;
		std::string _hFullscreen;
		std::string _empty;
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
		void	initFlowerResources();
		void	initDebugTextBox();
		void	initHelpTextBox();
		void	initFramebuffers(FBODatas &pingFBO, int w, int h);
		void	initFboShaders();
		void	resetFrameBuffers();
		void	updateFboWindowSize(PostProcessShader &shader);
		void	initMsaaFramebuffers(FBODatas &fboData, int width, int height);
		void   initWireframeResources();
		void   renderAimHighlight();
		void   postProcessSkyboxComposite();
		void   setFullscreen(bool enable);
		void   renderLoadingScreen();
		void   updateHelpStatusText();

		// Debug/test helper to add an instance at runtime
		void   addFlower(glm::vec3 pos, int typeId=0, float rotJitter=0.0f, float scale=1.0f, float heightScale=1.0f);
		void   removeFlowerAtCell(const glm::ivec3& cell);
		void   rebuildVisibleFlowersVBO();

		// Runtime methods
		void calculateFps();
		void display();
		void renderSkybox();
		void renderOverlayAndUI();
		void finalizeFrame();
		void renderTransparentObjects();
		void renderSolidObjects();
		void renderFlowers();
		void resolveMsaaToFbo(FBODatas &destinationFBO, bool resolveDepth);
		void prepareRenderPipeline();
		void displaySun(FBODatas &targetFBO);
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
		void swapPingPongBuffers();
		void blitColor(FBODatas& src, FBODatas& dst);
		void blitColorDepth(FBODatas& src, FBODatas& dst);

		void screenshotFBOBuffer(FBODatas &source, FBODatas &destination);
		void postProcessGreedyFix();
		void postProcessFog();
		void postProcessGodRays();
		void sendPostProcessFBOToDispay(const FBODatas &sourceFBO);
	
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
