#pragma once

#include <unordered_set>
#include <queue>
#include <list>
#include <atomic>
#include <mutex>
#include <unordered_map>
#include <vector>
#include <limits>

#include "ft_vox.hpp"
#include "NoiseGenerator.hpp"
#include "SubChunk.hpp"
#include "CaveGenerator.hpp"
#include "Chunk.hpp"
#include "Camera.hpp"
#include "Chrono.hpp"
#include "ThreadPool.hpp"
#include "Frustum.hpp"
#include "ChunkLoader.hpp"
#include "ChunkRenderer.hpp"
#include "Raycaster.hpp"

class Chunk;

class ChunkLoader
{
private:
	// Modification queue for blocks breaking
	struct PendingBlock {
		glm::ivec3 worldPos;
		BlockType  value;
	};
	std::unordered_map<glm::ivec2, std::vector<PendingBlock>, ivec2_hash> _pendingEdits;
	
	// List of cached chunks in order
	std::list<Chunk *>	_chunkList;

	// Mutexes
	std::mutex	_pendingMutex;
	std::mutex	_chunksListMutex;
	std::mutex	_chunksMutex;
	std::mutex	_dirtyMutex;
	std::mutex	_frustumMutex;
	std::mutex	_drawDataMutex;
	std::mutex	_displayedChunksMutex;

	// Cached chunks
	std::unordered_map<ivec2, Chunk *, ivec2_hash>	_chunks;
	std::unordered_map<ivec2, Chunk *, ivec2_hash>	_displayedChunks;

	// Tracking memory usage of chunks
	size_t _chunksMemoryUsage;

	// Reference to camera for accurate chunk loading
	Camera &_camera;

	// World relative data
	int				_renderDistance;
	int				_currentRender;
	int				_maxRender;
	std::atomic_int	_threshold;

	// Common chrono for debug
	Chrono &_chronoHelper;

	// Common threadpool for the program
	ThreadPool &_threadPool;

	// Concurrency guard to avoid concurrent/stacked heavy builds
	std::atomic_bool	_buildingDisplay;
	std::atomic_bool	*_isRunning;

	// Terrain generation heavy lifters
	NoiseGenerator	_perlinGenerator;
	CaveGenerator	_caveGen;

	// Chunks edit tracking
	std::unordered_set<ivec2, ivec2_hash> _dirtyChunks;

	// Frustum loading data
	Frustum	_cachedFrustum;
	bool	_hasCachedFrustum = false;

	// Loaded chunks to be added to the display (common with ChunkRenderer)
	std::queue<DisplayData *>	&_solidStagedDataQueue;
	std::queue<DisplayData *>	&_transparentStagedDataQueue;
// Methods
private:
	// Init methods
	void initData();

	// Edits (queue if chunk not ready)
	void applyPendingFor(const ivec2& pos);

	// Runtime chunk loading/unloading
	Chunk *loadChunk(int x, int z, int render, ivec2 &chunkPos, int resolution);
	bool hasMoved(ivec2 &oldPos);
	void buildFacesToDisplay(DisplayData *fillData, DisplayData *transparentFillData);
	void updateFillData();

	// Chunks edit tracking
	void	flushDirtyChunks();
public:
	ChunkLoader(
		int seed,
		Camera &camera,
		ThreadPool &pool,
		Chrono &chronoHelper,
		std::atomic_bool *isRunning,
		std::queue<DisplayData *>	&solidStagedDataQueue,
		std::queue<DisplayData *>	&transparentStagedDataQueue
	);
	~ChunkLoader();

	// Init methods
	void initSpawn();

	// Runtime chunk loading/unloading/updating
	void	loadChunks(ivec2 &camPosition);
	void	unloadChunks(ivec2 &newCamChunk);
	void	scheduleDisplayUpdate();

	// Shared data getters
	Chunk*			getChunk(const ivec2 &position);
	SubChunk*		getSubChunk(ivec3 &position);
	BlockType		getBlock(ivec2 chunkPos, ivec3 worldPos);
	TopBlock		findTopBlockY(ivec2 chunkPos, ivec2 worldPos);
	TopBlock		findBlockUnderPlayer(ivec2 chunkPos, ivec3 worldPos);
	bool			getIsRunning();
	NoiseGenerator &getNoiseGenerator();
	void			getDisplayedChunksSnapshot(std::vector<glm::ivec2>& out);
	bool			hasRenderableChunks();

	// Shared data setters
	bool	setBlockOrQueue(ivec2 chunkPos, ivec3 worldPos, BlockType value);
	void	markChunkDirty(const ivec2& pos);
	bool	setBlock(ivec2 chunkPos, ivec3 worldPos, BlockType value);
	void	setViewProj(Frustum &f);

	// Debug shared data getters and prints
	size_t	*getMemorySizePtr();
	int		*getRenderDistancePtr();
	int		*getCurrentRenderPtr();
	void	printSizes() const;
};
