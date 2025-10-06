#include "Raycaster.hpp"

Raycaster::Raycaster(ChunkLoader &chunkLoader, Camera &camera) : _chunkLoader(chunkLoader), _camera(camera) { }
Raycaster::~Raycaster() { }

static inline bool isSolidDeletable(BlockType b) {
	// Flowers are deletable but non-solid in physics; still considered here
	return (b != AIR && b != WATER && b != BEDROCK);
}

// Billboarded, non-solid plants rendered via the flower pipeline
static inline bool isFlower(BlockType b) {
	return (b == FLOWER_POPPY
		 || b == FLOWER_DANDELION
		 || b == FLOWER_CYAN
		 || b == FLOWER_SHORT_GRASS
		 || b == FLOWER_DEAD_BUSH);
}

// Any plant-like block that obeys the flowerPlaceCondition rules
static inline bool isPlantPlaceable(BlockType b) {
	return isFlower(b) || b == CACTUS;
}

static inline bool flowerPlaceCondition(BlockType support, BlockType placing)
{
	return (
		// Flowers and grass on grass block and dirt
		((support == GRASS || support == DIRT) &&
			(placing == FLOWER_POPPY
			|| placing == FLOWER_DANDELION
			|| placing == FLOWER_CYAN
			|| placing == FLOWER_SHORT_GRASS))
		// Cactus and dead bush on sand
		|| ((support == SAND) &&
			(placing == FLOWER_DEAD_BUSH || placing == CACTUS))
		// Cactus on cactus
		|| ((support == CACTUS) &&
			(placing == CACTUS))
	);
}

bool Raycaster::raycastHit(const glm::vec3& originWorld,
						const glm::vec3& dirWorld,
						float maxDistance,
						glm::ivec3& outBlock)
{
	if (maxDistance <= 0.0f) return false;

	glm::vec3 dir = dirWorld;
	float dlen = glm::length(dir);
	if (dlen < 1e-8f) return false;
	dir /= dlen;

	glm::vec3 ro = originWorld + dir * 0.001f; // nudge
	glm::ivec3 voxel(
		(int)std::floor(ro.x),
		(int)std::floor(ro.y),
		(int)std::floor(ro.z)
	);

	glm::ivec3 step(
		(dir.x > 0.f) ? 1 : (dir.x < 0.f) ? -1 : 0,
		(dir.y > 0.f) ? 1 : (dir.y < 0.f) ? -1 : 0,
		(dir.z > 0.f) ? 1 : (dir.z < 0.f) ? -1 : 0
	);

	const float INF = std::numeric_limits<float>::infinity();
	glm::vec3 tDelta(
		(step.x != 0) ? 1.0f / std::abs(dir.x) : INF,
		(step.y != 0) ? 1.0f / std::abs(dir.y) : INF,
		(step.z != 0) ? 1.0f / std::abs(dir.z) : INF
	);

	auto firstBoundaryT = [&](float r, int v, int s, float d) -> float {
		if (s > 0) return ((float(v) + 1.0f) - r) / d;
		if (s < 0) return (r - float(v)) / -d;
		return INF;
	};
	glm::vec3 tMaxV(
		firstBoundaryT(ro.x, voxel.x, step.x, dir.x),
		firstBoundaryT(ro.y, voxel.y, step.y, dir.y),
		firstBoundaryT(ro.z, voxel.z, step.z, dir.z)
	);

	float t = 0.0f;
	const int MAX_STEPS = 512;

	for (int i = 0; i < MAX_STEPS; ++i) {
		// step to next voxel
		if (tMaxV.x < tMaxV.y) {
			if (tMaxV.x < tMaxV.z)	{ voxel.x += step.x; t = tMaxV.x; tMaxV.x += tDelta.x; }
			else					{ voxel.z += step.z; t = tMaxV.z; tMaxV.z += tDelta.z; }
		} else {
			if (tMaxV.y < tMaxV.z)	{ voxel.y += step.y; t = tMaxV.y; tMaxV.y += tDelta.y; }
			else					{ voxel.z += step.z; t = tMaxV.z; tMaxV.z += tDelta.z; }
		}
		if (t > maxDistance) return false;

		ivec2 chunkPos(
			(int)std::floor((float)voxel.x / (float)CHUNK_SIZE),
			(int)std::floor((float)voxel.z / (float)CHUNK_SIZE)
		);
		BlockType b = _chunkLoader.getBlock(chunkPos, voxel);
		if (isSolidDeletable(b)) {
			outBlock = voxel;
			return true;
		}
	}
	return false;
}

BlockType Raycaster::raycastHitFetch(const glm::vec3& originWorld,
						const glm::vec3& dirWorld,
						float maxDistance,
						glm::ivec3& outBlock)
{
	if (maxDistance <= 0.0f) return AIR;

	glm::vec3 dir = dirWorld;
	float dlen = glm::length(dir);
	if (dlen < 1e-8f) return AIR;
	dir /= dlen;

	glm::vec3 ro = originWorld + dir * 0.001f; // nudge
	glm::ivec3 voxel(
		(int)std::floor(ro.x),
		(int)std::floor(ro.y),
		(int)std::floor(ro.z)
	);

	glm::ivec3 step(
		(dir.x > 0.f) ? 1 : (dir.x < 0.f) ? -1 : 0,
		(dir.y > 0.f) ? 1 : (dir.y < 0.f) ? -1 : 0,
		(dir.z > 0.f) ? 1 : (dir.z < 0.f) ? -1 : 0
	);

	const float INF = std::numeric_limits<float>::infinity();
	glm::vec3 tDelta(
		(step.x != 0) ? 1.0f / std::abs(dir.x) : INF,
		(step.y != 0) ? 1.0f / std::abs(dir.y) : INF,
		(step.z != 0) ? 1.0f / std::abs(dir.z) : INF
	);

	auto firstBoundaryT = [&](float r, int v, int s, float d) -> float {
		if (s > 0) return ((float(v) + 1.0f) - r) / d;
		if (s < 0) return (r - float(v)) / -d;
		return INF;
	};
	glm::vec3 tMaxV(
		firstBoundaryT(ro.x, voxel.x, step.x, dir.x),
		firstBoundaryT(ro.y, voxel.y, step.y, dir.y),
		firstBoundaryT(ro.z, voxel.z, step.z, dir.z)
	);

	float t = 0.0f;
	const int MAX_STEPS = 512;

	for (int i = 0; i < MAX_STEPS; ++i) {
		// step to next voxel
		if (tMaxV.x < tMaxV.y) {
			if (tMaxV.x < tMaxV.z)	{ voxel.x += step.x; t = tMaxV.x; tMaxV.x += tDelta.x; }
			else					{ voxel.z += step.z; t = tMaxV.z; tMaxV.z += tDelta.z; }
		} else {
			if (tMaxV.y < tMaxV.z)	{ voxel.y += step.y; t = tMaxV.y; tMaxV.y += tDelta.y; }
			else					{ voxel.z += step.z; t = tMaxV.z; tMaxV.z += tDelta.z; }
		}
		if (t > maxDistance) return AIR;

		ivec2 chunkPos(
			(int)std::floor((float)voxel.x / (float)CHUNK_SIZE),
			(int)std::floor((float)voxel.z / (float)CHUNK_SIZE)
		);
		BlockType b = _chunkLoader.getBlock(chunkPos, voxel);
		if (isSolidDeletable(b)) {
			outBlock = voxel;
			return b;
		}
	}
	return AIR;
}

bool Raycaster::raycastDeleteOne(const glm::vec3& originWorld,
				const glm::vec3& dirWorld,
				float maxDistance)
{
	glm::ivec3 hit;
	if (!raycastHit(originWorld, dirWorld, maxDistance, hit))
		return false;

	// Chunk that owns the hit voxel
	ivec2 chunkPos(
		(int)std::floor((float)hit.x / (float)CHUNK_SIZE),
		(int)std::floor((float)hit.z / (float)CHUNK_SIZE)
	);

	// Fetch block type before deletion (for cascading behavior)
	BlockType hitTypeBefore = _chunkLoader.getBlock(chunkPos, hit);

	// Try to apply immediately (falls back to queue if chunk not ready)
	bool wroteNow = _chunkLoader.setBlockOrQueue(chunkPos, hit, AIR, /*byPlayer=*/true);

	if (wroteNow)
	{
		// 1) Rebuild current chunk mesh
		if (Chunk* c = _chunkLoader.getChunk(chunkPos))
		{
			c->setAsModified();
			c->sendFacesToDisplay();
		}

		// 2) If at border, rebuild neighbor meshes that share the broken face
		const int lx = (hit.x % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;
		const int lz = (hit.z % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;

		ivec2 neighbors[4];
		int n = 0;
		if (lx == 0)               neighbors[n++] = { chunkPos.x - 1, chunkPos.y };
		if (lx == CHUNK_SIZE - 1)  neighbors[n++] = { chunkPos.x + 1, chunkPos.y };
		if (lz == 0)               neighbors[n++] = { chunkPos.x,     chunkPos.y - 1 };
		if (lz == CHUNK_SIZE - 1)  neighbors[n++] = { chunkPos.x,     chunkPos.y + 1 };

		for (int i = 0; i < n; ++i)
			if (Chunk* nc = _chunkLoader.getChunk(neighbors[i]))
				nc->sendFacesToDisplay();

		// 3) Stage a fresh display snapshot off-thread (coalesced)
		_chunkLoader.scheduleDisplayUpdate();
	}

	// If the broken block was a support block, handle any plant sitting above
	if (hitTypeBefore == SAND || hitTypeBefore == DIRT || hitTypeBefore == GRASS)
	{
		glm::ivec3 above = hit + glm::ivec3(0, 1, 0);
		ivec2 aboveChunk(
			(int)std::floor((float)above.x / (float)CHUNK_SIZE),
			(int)std::floor((float)above.z / (float)CHUNK_SIZE)
		);
		BlockType aboveType = _chunkLoader.getBlock(aboveChunk, above);

		if (hitTypeBefore == SAND && aboveType == CACTUS)
		{
			// Cascade remove the entire cactus column above the broken sand
			glm::ivec3 cur = above;
			while (true)
			{
				ivec2 curChunk(
					(int)std::floor((float)cur.x / (float)CHUNK_SIZE),
					(int)std::floor((float)cur.z / (float)CHUNK_SIZE)
				);
				BlockType curType = _chunkLoader.getBlock(curChunk, cur);
				if (curType != CACTUS) break;

				bool wrote = _chunkLoader.setBlockOrQueue(curChunk, cur, AIR, /*byPlayer=*/true);
				if (wrote)
				{
					if (Chunk* c2 = _chunkLoader.getChunk(curChunk))
					{
						c2->setAsModified();
						c2->sendFacesToDisplay();
					}

					const int lx2 = (cur.x % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;
					const int lz2 = (cur.z % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;
					ivec2 neighbors2[4];
					int n2 = 0;
					if (lx2 == 0)               neighbors2[n2++] = { curChunk.x - 1, curChunk.y };
					if (lx2 == CHUNK_SIZE - 1)  neighbors2[n2++] = { curChunk.x + 1, curChunk.y };
					if (lz2 == 0)               neighbors2[n2++] = { curChunk.x,     curChunk.y - 1 };
					if (lz2 == CHUNK_SIZE - 1)  neighbors2[n2++] = { curChunk.x,     curChunk.y + 1 };
					for (int i = 0; i < n2; ++i)
						if (Chunk* nc2 = _chunkLoader.getChunk(neighbors2[i]))
							nc2->sendFacesToDisplay();
				}

				// Move upward
				cur.y += 1;
			}

			// Coalesce display update once
			_chunkLoader.scheduleDisplayUpdate();
		}
		else if (isFlower(aboveType))
		{
			// Remove a single billboard plant above when breaking its support
			bool wroteTop = _chunkLoader.setBlockOrQueue(aboveChunk, above, AIR, /*byPlayer=*/true);
			if (wroteTop)
			{
				if (Chunk* c2 = _chunkLoader.getChunk(aboveChunk))
				{
					c2->setAsModified();
					c2->sendFacesToDisplay();
				}

				const int lx2 = (above.x % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;
				const int lz2 = (above.z % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;
				ivec2 neighbors2[4];
				int n2 = 0;
				if (lx2 == 0)               neighbors2[n2++] = { aboveChunk.x - 1, aboveChunk.y };
				if (lx2 == CHUNK_SIZE - 1)  neighbors2[n2++] = { aboveChunk.x + 1, aboveChunk.y };
				if (lz2 == 0)               neighbors2[n2++] = { aboveChunk.x,     aboveChunk.y - 1 };
				if (lz2 == CHUNK_SIZE - 1)  neighbors2[n2++] = { aboveChunk.x,     aboveChunk.y + 1 };
				for (int i = 0; i < n2; ++i)
					if (Chunk* nc2 = _chunkLoader.getChunk(neighbors2[i]))
						nc2->sendFacesToDisplay();

				_chunkLoader.scheduleDisplayUpdate();
			}
		}
	}

	// If we directly broke a cactus block, cascade-remove all cactus blocks above it
	if (hitTypeBefore == CACTUS)
	{
		glm::ivec3 cur = hit + glm::ivec3(0, 1, 0);
		while (true)
		{
			ivec2 curChunk(
				(int)std::floor((float)cur.x / (float)CHUNK_SIZE),
				(int)std::floor((float)cur.z / (float)CHUNK_SIZE)
			);
			BlockType curType = _chunkLoader.getBlock(curChunk, cur);
			if (curType != CACTUS) break;

			bool wrote = _chunkLoader.setBlockOrQueue(curChunk, cur, AIR, /*byPlayer=*/true);
			if (wrote)
			{
				if (Chunk* c2 = _chunkLoader.getChunk(curChunk))
				{
					c2->setAsModified();
					c2->sendFacesToDisplay();
				}

				const int lx2 = (cur.x % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;
				const int lz2 = (cur.z % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;
				ivec2 neighbors2[4];
				int n2 = 0;
				if (lx2 == 0)               neighbors2[n2++] = { curChunk.x - 1, curChunk.y };
				if (lx2 == CHUNK_SIZE - 1)  neighbors2[n2++] = { curChunk.x + 1, curChunk.y };
				if (lz2 == 0)               neighbors2[n2++] = { curChunk.x,     curChunk.y - 1 };
				if (lz2 == CHUNK_SIZE - 1)  neighbors2[n2++] = { curChunk.x,     curChunk.y + 1 };
				for (int i = 0; i < n2; ++i)
					if (Chunk* nc2 = _chunkLoader.getChunk(neighbors2[i]))
						nc2->sendFacesToDisplay();
			}

			cur.y += 1;
		}

		_chunkLoader.scheduleDisplayUpdate();
	}
	return true;
}

bool Raycaster::raycastPlaceOne(const glm::vec3& originWorld,
				const glm::vec3& dirWorld,
				float maxDistance,
				BlockType block,
				glm::ivec3& outPlaced)
{
	// Perform a DDA raycast and place the block on the empty voxel
		// in front of the hit block (the voxel before entering
	// the solid block), matching Minecraft behavior.

	if (maxDistance <= 0.0f) return false;

	// Normalize direction
	glm::vec3 dir = dirWorld;
	float dlen = glm::length(dir);
	if (dlen < 1e-8f) return false;
	dir /= dlen;

	// Nudge origin slightly to avoid self-intersection at boundaries
	glm::vec3 ro = originWorld + dir * 0.001f;

		// Current voxel and the previous voxel before stepping
	glm::ivec3 voxel(
		(int)std::floor(ro.x),
		(int)std::floor(ro.y),
		(int)std::floor(ro.z)
	);
	glm::ivec3 prev = voxel;

	// Step for each axis
	glm::ivec3 step(
		(dir.x > 0.f) ? 1 : (dir.x < 0.f) ? -1 : 0,
		(dir.y > 0.f) ? 1 : (dir.y < 0.f) ? -1 : 0,
		(dir.z > 0.f) ? 1 : (dir.z < 0.f) ? -1 : 0
	);

	const float INF = std::numeric_limits<float>::infinity();
	glm::vec3 tDelta(
		(step.x != 0) ? 1.0f / std::abs(dir.x) : INF,
		(step.y != 0) ? 1.0f / std::abs(dir.y) : INF,
		(step.z != 0) ? 1.0f / std::abs(dir.z) : INF
	);

	auto firstBoundaryT = [&](float r, int v, int s, float d) -> float {
		if (s > 0) return ((float(v) + 1.0f) - r) / d;
		if (s < 0) return (r - float(v)) / -d;
		return INF;
	};

	glm::vec3 tMaxV(
		firstBoundaryT(ro.x, voxel.x, step.x, dir.x),
		firstBoundaryT(ro.y, voxel.y, step.y, dir.y),
		firstBoundaryT(ro.z, voxel.z, step.z, dir.z)
	);

	float t = 0.0f;
	const int MAX_STEPS = 512;

	for (int i = 0; i < MAX_STEPS; ++i) {
			// Step to next voxel and record the previous voxel
		if (tMaxV.x < tMaxV.y) {
			if (tMaxV.x < tMaxV.z) { prev = voxel; voxel.x += step.x; t = tMaxV.x; tMaxV.x += tDelta.x; }
			else                   { prev = voxel; voxel.z += step.z; t = tMaxV.z; tMaxV.z += tDelta.z; }
		} else {
			if (tMaxV.y < tMaxV.z) { prev = voxel; voxel.y += step.y; t = tMaxV.y; tMaxV.y += tDelta.y; }
			else                   { prev = voxel; voxel.z += step.z; t = tMaxV.z; tMaxV.z += tDelta.z; }
		}

		if (t > maxDistance) return false;

		ivec2 chunkPos(
			(int)std::floor((float)voxel.x / (float)CHUNK_SIZE),
			(int)std::floor((float)voxel.z / (float)CHUNK_SIZE)
		);
		BlockType b = _chunkLoader.getBlock(chunkPos, voxel);

		// Special case: if we hit a decorative plant (billboard) and the player is placing a non-flower, non-air block,
		// replace the flower instead of placing above it.
		if (isFlower(b)) {
			bool placingNonFlower = (block != AIR && !isFlower(block));
			if (placingNonFlower) {
				// Prevent placing a block inside the player's occupied cells (feet/head)
				{
					glm::vec3 camW = _camera.getWorldPosition();
					int px = static_cast<int>(std::floor(camW.x));
					int pz = static_cast<int>(std::floor(camW.z));
					int footY = static_cast<int>(std::floor(camW.y - EYE_HEIGHT + EPS));
					glm::ivec3 feet = { px, footY, pz };
					glm::ivec3 head = { px, footY + 1, pz };
					if (voxel == feet || voxel == head) {
						return false; // would collide with player
					}
				}

				ivec2 placeChunk(
					(int)std::floor((float)voxel.x / (float)CHUNK_SIZE),
					(int)std::floor((float)voxel.z / (float)CHUNK_SIZE)
				);

				bool wroteNow = _chunkLoader.setBlockOrQueue(placeChunk, voxel, block, /*byPlayer=*/true);
				if (wroteNow) {
					// Rebuild current chunk mesh
					if (Chunk* c = _chunkLoader.getChunk(placeChunk))
					{
						c->setAsModified();
						c->sendFacesToDisplay();
					}

					// If at border, also rebuild neighbors that share faces
					const int lx = (voxel.x % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;
					const int lz = (voxel.z % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;

					ivec2 neighbors[4];
					int n = 0;
					if (lx == 0)               neighbors[n++] = { placeChunk.x - 1, placeChunk.y };
					if (lx == CHUNK_SIZE - 1)  neighbors[n++] = { placeChunk.x + 1, placeChunk.y };
					if (lz == 0)               neighbors[n++] = { placeChunk.x,     placeChunk.y - 1 };
					if (lz == CHUNK_SIZE - 1)  neighbors[n++] = { placeChunk.x,     placeChunk.y + 1 };

					for (int i2 = 0; i2 < n; ++i2)
						if (Chunk* nc = _chunkLoader.getChunk(neighbors[i2]))
							nc->sendFacesToDisplay();

					// Stage a fresh display snapshot off-thread (coalesced)
					_chunkLoader.scheduleDisplayUpdate();
				}

				outPlaced = voxel;
				return true;
			}
			// else: placing a flower or air -> fall through to default behavior (place on prev if empty)
		}

		if (b == BEDROCK || isSolidDeletable(b)) {
			// Place into the previous (empty) voxel before entering the hit voxel
			ivec2 placeChunk(
				(int)std::floor((float)prev.x / (float)CHUNK_SIZE),
				(int)std::floor((float)prev.z / (float)CHUNK_SIZE)
			);

			// Prevent placing a block inside the player's occupied cells (feet/head)
			{
				glm::vec3 camW = _camera.getWorldPosition();
				int px = static_cast<int>(std::floor(camW.x));
				int pz = static_cast<int>(std::floor(camW.z));
				int footY = static_cast<int>(std::floor(camW.y - EYE_HEIGHT + EPS));
				glm::ivec3 feet = { px, footY, pz };
				glm::ivec3 head = { px, footY + 1, pz };
				if (prev == feet || prev == head) {
					return false; // would collide with player
				}
			}

			// Only place if target is empty or replaceable (AIR/WATER).
			BlockType current = _chunkLoader.getBlock(placeChunk, prev);

			// If placing a flower/plant, use flowerPlaceCondition rules:
			// - Target cell must be AIR (not water)
			// - Support below must satisfy flowerPlaceCondition for the block type
			if (isPlantPlaceable(block)) {
				if (current != AIR) return false;
				glm::ivec3 below = prev; below.y -= 1;
				ivec2 belowChunk(
					(int)std::floor((float)below.x / (float)CHUNK_SIZE),
					(int)std::floor((float)below.z / (float)CHUNK_SIZE)
				);
				BlockType support = _chunkLoader.getBlock(belowChunk, below);
				if (!flowerPlaceCondition(support, block)) return false;
			} else {
				if (!(current == AIR || current == WATER)) return false;
			}

			bool wroteNow = _chunkLoader.setBlockOrQueue(placeChunk, prev, block, /*byPlayer=*/true);
			if (wroteNow) {
				// Rebuild current chunk mesh
				if (Chunk* c = _chunkLoader.getChunk(placeChunk))
				{
					c->setAsModified();
					c->sendFacesToDisplay();
				}

				// If at border, also rebuild neighbors that share faces
				const int lx = (prev.x % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;
				const int lz = (prev.z % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;

				ivec2 neighbors[4];
				int n = 0;
				if (lx == 0)               neighbors[n++] = { placeChunk.x - 1, placeChunk.y };
				if (lx == CHUNK_SIZE - 1)  neighbors[n++] = { placeChunk.x + 1, placeChunk.y };
				if (lz == 0)               neighbors[n++] = { placeChunk.x,     placeChunk.y - 1 };
				if (lz == CHUNK_SIZE - 1)  neighbors[n++] = { placeChunk.x,     placeChunk.y + 1 };

				for (int i2 = 0; i2 < n; ++i2)
					if (Chunk* nc = _chunkLoader.getChunk(neighbors[i2]))
						nc->sendFacesToDisplay();

				// Stage a fresh display snapshot off-thread (coalesced)
				_chunkLoader.scheduleDisplayUpdate();
			}

			// Return the actual placed cell from this first pass
			outPlaced = prev;
			return true;
		}
	}
	return false;
}

bool Raycaster::raycastPlaceOne(const glm::vec3& originWorld,
				const glm::vec3& dirWorld,
				float maxDistance,
				BlockType block)
{
	glm::ivec3 dummy;
	return raycastPlaceOne(originWorld, dirWorld, maxDistance, block, dummy);
}
