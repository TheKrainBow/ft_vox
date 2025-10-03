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
		bool       byPlayer; // true if edit comes from player action
	};
	std::unordered_map<glm::ivec2, std::vector<PendingBlock>, ivec2_hash> _pendingEdits;

	// LRU list of cached chunks (oldest at front)
	std::list<ivec2> _chunkList;
	// Map chunk position to LRU iterator for O(1) updates
	std::unordered_map<ivec2, std::list<ivec2>::iterator, ivec2_hash> _lruIndex;

	// Mutexes
	std::mutex	_pendingMutex;
	std::mutex	_chunksListMutex;
	std::mutex	_chunksMutex;
	std::mutex	_dirtyMutex;
	std::mutex	_frustumMutex;
	// Shared mutex guarding staged draw-data queues (shared with ChunkRenderer)
	std::mutex	&_sharedDrawDataMutex;
	std::mutex	_displayedChunksMutex;

	// Cached chunks
	std::unordered_map<ivec2, Chunk *, ivec2_hash>	_chunks;
	std::unordered_map<ivec2, Chunk *, ivec2_hash>	_displayedChunks;

	// Tracking memory usage of chunks (atomic for cross-thread updates)
	std::atomic_size_t _chunksMemoryUsage;
	int    _countBudget;

	// Live counters (atomic) updated by worker/render threads
	std::atomic_int _chunksCount{0};
	std::atomic_int _displayedCount{0};
	std::atomic_int _modifiedCount{0};

	// Reference to camera for accurate chunk loading
	Camera &_camera;

	// World relative data (atomics to avoid data races)
	std::atomic_int		_renderDistance;
	std::atomic_int		_currentRender;
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

    // Flowers discovered in world data (by subchunk), staged for the renderer
    // Map: chunk -> subY -> list of (cell, type)
    std::unordered_map<glm::ivec2, std::unordered_map<int, std::vector<std::pair<glm::ivec3, BlockType>>>, ivec2_hash> _discoveredFlowers;
    // Guard for discovered flower structures
    std::mutex _flowersMutex;
    // Track which sublayers have been scanned to avoid re-scanning/duplicating
    std::unordered_set<std::string> _flowersScannedKeys;
    static std::string _flowerKey(const glm::ivec2& cpos, int subY) {
        return std::to_string(cpos.x) + ":" + std::to_string(cpos.y) + ":" + std::to_string(subY);
    }
    // Snapshot of currently displayed subchunks per chunk (from last build)
    std::unordered_map<glm::ivec2, std::unordered_set<int>, ivec2_hash> _lastDisplayedSubY;
	// Debug snapshot values exposed to UI (stable addresses, written on main thread)
	size_t _dbg_chunksMemoryUsage{0};
	int    _dbg_renderDistance{0};
	int    _dbg_currentRender{0};
	int    _dbg_chunksCount{0};
	int    _dbg_displayedCount{0};
	int    _dbg_modifiedCount{0};

	// Methods
private:
	// Init methods
	void initData();

	// Edits (queue if chunk not ready)
	void applyPendingFor(const ivec2& pos);

	// Runtime chunk loading/unloading
    Chunk *loadChunk(int x, int z, int render, const ivec2 &chunkPos, int resolution);
    bool hasMoved(const ivec2 &oldPos);
	void buildFacesToDisplay(DisplayData *fillData, DisplayData *transparentFillData);
	void updateFillData();

	// Chunks edit tracking
	void	flushDirtyChunks();

	// LRU + cache budget helpers
	void touchLRU(const ivec2& pos);
    void enforceCountBudget();
    bool evictChunkAt(const ivec2& pos);
public:
	ChunkLoader(
		int seed,
		Camera &camera,
		ThreadPool &pool,
		Chrono &chronoHelper,
		std::atomic_bool *isRunning,
		std::mutex &sharedDrawDataMutex,
		std::queue<DisplayData *>	&solidStagedDataQueue,
		std::queue<DisplayData *>	&transparentStagedDataQueue
	);
	~ChunkLoader();

	// Init methods
	void initSpawn();

	// Runtime chunk loading/unloading/updating
    void	loadChunks(ivec2 camPosition);
    void	unloadChunks(ivec2 newCamChunk);
	void	scheduleDisplayUpdate();
	// Snapshot atomics into UI-visible plain fields (call on main thread)
	void	snapshotDebugCounters();

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
	bool	setBlockOrQueue(ivec2 chunkPos, ivec3 worldPos, BlockType value, bool byPlayer = true);
	void	markChunkDirty(const ivec2& pos);
	bool	setBlock(ivec2 chunkPos, ivec3 worldPos, BlockType value, bool byPlayer);
    void	setViewProj(Frustum &f);

    // Flowers: scan a subchunk once and stage its flower cells for the renderer
    void scanAndRecordFlowersFor(const glm::ivec2& cpos, int subY, class SubChunk* sc, int resolution);
    // Flowers: fetch and clear all staged flower cells
    void fetchAndClearDiscoveredFlowers(std::vector<std::tuple<glm::ivec2,int,glm::ivec3,BlockType>>& out);

    // Visible subchunks snapshot for external culling (e.g., flower instances)
    void getDisplayedSubchunksSnapshot(std::unordered_map<glm::ivec2, std::unordered_set<int>, ivec2_hash>& out);

	// Debug shared data getters and prints
	size_t	*getMemorySizePtr();
	int		*getRenderDistancePtr();
	int		*getCurrentRenderPtr();
	int		*getCachedChunksCountPtr();
	int		*getDisplayedChunksCountPtr();
	int		*getModifiedChunksCountPtr();
	void	printSizes() const;
};
