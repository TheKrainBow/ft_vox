#pragma once
#include <cstdint>
#include <functional>
#include <vector>

struct CaveWaterSettings {
	float caveWaterFrequency = 1.0; // Chance per 48^3 feature region, before validation.
	int caveWaterMinY = 12, caveWaterMaxY = 180; // Maximum is exclusive.
	float caveWaterPoolChance = 0.75f, caveWaterPocketChance = 0.25f;
	float caveWaterSpringChance = 0.25f; // Relative weight for single-source wall outlets.
	int caveWaterMinRadius = 2, caveWaterMaxRadius = 10;
	int regionSize = 48, minimumUndergroundDepth = 24;
	int maxVisitedVoxels = 2048;
};

struct CaveWaterPos {
	int x, y, z;
	bool operator==(const CaveWaterPos& p) const { return x == p.x && y == p.y && z == p.z; }
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
	std::vector<CaveWaterPos> generate(CaveWaterPos min, CaveWaterPos max, const Sample& sample) const;
private:
	uint64_t m_seed;
	CaveWaterSettings m_settings;
};
