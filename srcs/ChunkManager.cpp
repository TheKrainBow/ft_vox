#include "ChunkManager.hpp"

ChunkManager::ChunkManager(
	int seed,
	std::atomic_bool *isRunning,
	Camera &cam,
	Chrono &chrono,
	ThreadPool &pool
):
_seed(seed),
_isRunning(isRunning),
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
	_solidDrawDataMutex,
	_solidStagedDataQueue,
	_transparentStagedDataQueue
),
_chunkRenderer(
	_solidDrawDataMutex,
	_transparentDrawDataMutex,
	_solidStagedDataQueue,
	_transparentStagedDataQueue
),
_raycaster(_chunkLoader, _camera)
{
}

ChunkManager::~ChunkManager()
{
}

// Init method for spawnpoint
void ChunkManager::initSpawn()
{
	_chunkLoader.initSpawn();
}

// Init method for GL buffers
void ChunkManager::initGLBuffer()
{
	_chunkRenderer.initGLBuffer();
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

BlockType ChunkManager::getBlock(ivec2 chunkPos, ivec3 worldPos)
{
	return _chunkLoader.getBlock(chunkPos, worldPos);
}

void ChunkManager::getDisplayedChunksSnapshot(std::vector<ivec2>& out)
{
	_chunkLoader.getDisplayedChunksSnapshot(out);
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

void ChunkManager::setOcclusionSource(GLuint depthTex, int width, int height,
										const glm::mat4& view, const glm::mat4& proj,
										const glm::vec3& camPos)
{
	_chunkRenderer.setOcclusionSource(depthTex, width, height, view, proj, camPos);
}

void ChunkManager::setRendererSyncMode(bool enabled)
{
	_chunkRenderer.setSyncAfterDraw(enabled);
}

// Chunks loading and unloading methods
void ChunkManager::loadChunks(ivec2 &camPos)
{
	_chunkLoader.loadChunks(camPos);
}

void ChunkManager::unloadChunks(ivec2 &camPos)
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

// Raycasting (selecting, placing and deleting blocks)
bool ChunkManager::raycastHit(const glm::vec3& originWorld,
						const glm::vec3& dirWorld,
						float maxDistance,
						glm::ivec3& outBlock)
{
	return _raycaster.raycastHit(originWorld, dirWorld, maxDistance, outBlock);
}

BlockType ChunkManager::raycastHitFetch(const glm::vec3& originWorld,
						const glm::vec3& dirWorld,
						float maxDistance,
						glm::ivec3& outBlock)
{
	return _raycaster.raycastHitFetch(originWorld, dirWorld, maxDistance, outBlock);
}

bool ChunkManager::raycastDeleteOne(const glm::vec3& originWorld,
				const glm::vec3& dirWorld,
				float maxDistance)
{
	return _raycaster.raycastDeleteOne(originWorld, dirWorld, maxDistance);
}

bool ChunkManager::raycastPlaceOne(const glm::vec3& originWorld,
				const glm::vec3& dirWorld,
				float maxDistance,
				BlockType block)
{
	return _raycaster.raycastPlaceOne(originWorld, dirWorld, maxDistance, block);
}
