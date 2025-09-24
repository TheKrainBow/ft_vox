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

class Raycaster
{
private:
	// Chunk loader to be able to access chunk world data
	ChunkLoader	&_chunkLoader;

	// Camera shared data
	Camera		&_camera;
public:
	Raycaster(ChunkLoader &chunkLoader, Camera &_camera);
	~Raycaster();

	bool raycastHit(const glm::vec3& originWorld,
		const glm::vec3& dirWorld,
		float maxDistance,
		glm::ivec3& outBlock);
	BlockType raycastHitFetch(const glm::vec3& originWorld,
		const glm::vec3& dirWorld,
		float maxDistance,
		glm::ivec3& outBlock);
	bool raycastDeleteOne(const glm::vec3& originWorld,
		const glm::vec3& dirWorld,
		float maxDistance = 5.0f);
	bool raycastPlaceOne(const glm::vec3& originWorld,
				const glm::vec3& dirWorld,
				float maxDistance,
				BlockType block);
};