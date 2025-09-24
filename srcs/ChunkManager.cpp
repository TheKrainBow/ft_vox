#include "ChunkManager.hpp"

ChunkManager::ChunkManager(
	int seed,
	std::atomic_bool *isRunning,
	Camera &cam,
	Chrono &chrono,
	ThreadPool &pool
):
_seed(seed),
_camera(cam),
_chronoHelper(chrono),
_threadPool(pool),
_solidStagedDataQueue(),
_transparentStagedDataQueue(),
_chunkLoader(
	seed,
	_camera,
	_threadPool,
	_chronoHelper,
	_isRunning,
	_solidStagedDataQueue,
	_transparentStagedDataQueue
),
_chunkRenderer(
	_solidDrawDataMutex,
	_transparentDrawDataMutex,
	_solidStagedDataQueue,
	_transparentStagedDataQueue
)
{
}

ChunkManager::~ChunkManager()
{

}

// Rendering methods
int ChunkManager::renderSolidBlocks()
{
	return _chunkRenderer.renderSolidBlocks();
}

int ChunkManager::renderTransparentBlocks()
{
	return _chunkRenderer.renderTransparentBlocks();
}

// Init method for spawnpoint
void ChunkManager::initSpawn()
{
	_chunkLoader.initSpawn();
}

// Shutdown methods
void ChunkManager::shutDownGL()
{
	_chunkRenderer.shutdownGL();
}

// Shared data getters
size_t *ChunkManager::getMemorySizePtr() { 
	return _chunkLoader.getMemorySizePtr();
}

int	*ChunkManager::getRenderDistancePtr() { 
	return _chunkLoader.getRenderDistancePtr();
}

int	*ChunkManager::getCurrentRenderPtr() { 
	return _chunkLoader.getCurrentRenderPtr();
}

// Shared data setters
void ChunkManager::setViewProj(const glm::mat4& view, const glm::mat4& proj)
{
	Frustum f = Frustum::fromVP(proj * view);
	glm::vec4 planes[6];
	for (int i = 0; i < 6; ++i)
		planes[i] = glm::vec4(f.p[i].n, f.p[i].d);
	_chunkRenderer.setViewProj(planes);
	_chunkLoader.setViewProj(f);
}

// Chunks loading and unloading methods
void ChunkManager::loadChunks(ivec2 &camPos)
{
	_chunkLoader.loadChunks(camPos);
}

void ChunkManager::loadChunks(ivec2 &camPos)
{
	_chunkLoader.unloadChunks(camPos);
}

// Draw data swapper
void ChunkManager::updateDrawData()
{
	_chunkRenderer.updateDrawData();
}

// Mesh rendering methods
int ChunkManager::renderSolidBlocks()
{
	return _chunkRenderer.renderSolidBlocks();
}

int ChunkManager::renderTransparentBlocks()
{
	return _chunkRenderer.renderTransparentBlocks();
}

// Collisions helper
TopBlock ChunkManager::findBlockUnderPlayer(ivec2 chunkPos, ivec3 worldPos)
{
	return _chunkLoader.findBlockUnderPlayer(chunkPos, worldPos);
}

// Cache size debugging
void ChunkManager::printSizes() const
{
	_chunkLoader.printSizes();
}
