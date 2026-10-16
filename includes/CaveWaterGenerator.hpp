#pragma once
#include <cstdint>
#include <functional>
#include <vector>
#include <array>
#include <map>
#include <mutex>
#include <future>
#include <cstddef>

struct CaveWaterSettings {
	float caveWaterFrequency = 1.4f; // Expected candidate attempts per 48^3 region (0..4).
	int caveWaterMinY = 12, caveWaterMaxY = 180; // Maximum is exclusive.
	float caveWaterPoolChance = 0.75f, caveWaterPocketChance = 0.25f;
	float caveWaterSpringChance = 0.25f; // Relative weight for single-source wall outlets.
	int caveWaterMinRadius = 2, caveWaterMaxRadius = 16;
	int regionSize = 48, minimumUndergroundDepth = 24;
	int maxVisitedVoxels = 8192;
	float largePoolChance = 0.6f;
	int smallPoolMaxRadius = 5, largePoolMinRadius = 8, maxDepth = 6;
	float deepFrequencyMultiplier = 1, upperFrequencyMultiplier = 1;
	int springClearanceRadius = 0; // Lava additionally protects its potential runoff.
};

struct CaveWaterPos {
	int x, y, z;
	bool operator==(const CaveWaterPos& p) const { return x == p.x && y == p.y && z == p.z; }
};
struct CaveWaterPosHash {
	size_t operator()(CaveWaterPos p) const {
		uint64_t h = uint64_t(uint32_t(p.x))*0x9e3779b185ebca87ULL
			^ uint64_t(uint32_t(p.y))*0xc2b2ae3d27d4eb4fULL
			^ uint64_t(uint32_t(p.z))*0x165667b19e3779f9ULL;
		h ^= h >> 30;
		h *= 0xbf58476d1ce4e5b9ULL;
		return size_t(h ^ (h >> 27));
	}
};
// Protected includes surface terrain, unavailable geometry and bedrock.
enum class CaveWaterVoxel { Solid, Air, Protected };

class CaveWaterGenerator {
public:
	using Sample = std::function<CaveWaterVoxel(CaveWaterPos)>;
	explicit CaveWaterGenerator(uint64_t seed, CaveWaterSettings settings = {});
	uint64_t seed() const { return m_seed; }
	const CaveWaterSettings& settings() const { return m_settings; }
	// Half-open output box. Analysis uses world geometry, never loaded neighbors.
	// Cache only with an immutable world-geometry sampler. Test/custom samplers
	// retain uncached semantics by default.
	std::vector<CaveWaterPos> generate(CaveWaterPos min, CaveWaterPos max, const Sample& sample, bool cacheFeatures = false) const;
private:
	mutable std::mutex m_cacheMutex;
	mutable std::map<std::array<int,3>,std::shared_future<std::vector<CaveWaterPos>>> m_features;
	uint64_t m_seed;
	CaveWaterSettings m_settings;
};
