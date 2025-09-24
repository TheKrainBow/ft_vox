#pragma once

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

#include <unordered_set>
#include <queue>
#include <list>
#include <atomic>
#include <mutex>
#include <unordered_map>
#include <vector>
#include <limits>

class ChunkManager
{
private:
	// Seed for world generation
	int	_seed;

	// Running state for game loop
	std::atomic_bool	*_isRunning;

	// Loaded chunks to be added to the display (common with ChunkRenderer)
	std::queue<DisplayData *>	_solidStagedDataQueue;
	std::queue<DisplayData *>	_transparentStagedDataQueue;

	// Reference to camera for accurate chunk loading
	Camera &_camera;

	// Common chrono for debug
	Chrono &_chronoHelper;

	// Common threadpool for the program
	ThreadPool &_threadPool;

	// Common mutexes between ChunkLoader and ChunkRenderer
	std::mutex	_solidDrawDataMutex;
	std::mutex	_transparentDrawDataMutex;

	// Frustum view based chunk loading
	ChunkLoader _chunkLoader;

	// Chunk rendering helper
	ChunkRenderer _chunkRenderer;
public:
	ChunkManager(
		int seed,
		std::atomic_bool *isRunning,
		Camera &cam,
		Chrono &chrono,
		ThreadPool &pool
	);
	~ChunkManager();

	// Rendering methods
	int	renderSolidBlocks();
	int	renderTransparentBlocks();

	// Init method
	void	initSpawn();

	// Shutdown methods
	void	shutDownGL();

	// Shared data getters
	size_t		*getMemorySizePtr();
	int			*getRenderDistancePtr();
	int			*getCurrentRenderPtr();
	void		getDisplayedChunksSnapshot(std::vector<ivec2>& out);
	BlockType	getBlock(ivec2 chunkPos, ivec3 worldPos);

	// View projection setter for renderer
	void	setViewProj(const glm::mat4& view, const glm::mat4& proj);

	// Chunks loading and unloading methods
	void loadChunks(ivec2 &camPos);
	void unloadChunks(ivec2 &camPos);

	// Draw data swapper
	void updateDrawData();

	// Mesh rendering methods
	int renderSolidBlocks();
	int renderTransparentBlocks();

	// Collisions helper
	TopBlock findBlockUnderPlayer(ivec2 chunkPos, ivec3 worldPos);

	// Cache size debugging
	void printSizes() const;
};