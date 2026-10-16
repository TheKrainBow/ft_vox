#include "CaveGenerator.hpp"
#include <array>
#include <atomic>

namespace {
float smooth(float low, float high, float value) {
	float t = std::clamp((value - low) / (high - low), 0.0f, 1.0f);
	return t * t * (3 - 2 * t);
}
uint64_t mix(uint64_t v) {
	v += 0x9e3779b97f4a7c15ULL;
	v = (v ^ (v >> 30)) * 0xbf58476d1ce4e5b9ULL;
	v = (v ^ (v >> 27)) * 0x94d049bb133111ebULL;
	return v ^ (v >> 31);
}
}

CaveGenerator::CaveGenerator(uint64_t seed, CaveGenerationSettings settings)
	: m_settings(settings) {
	static std::atomic<uint64_t> nextId{1};
	m_cacheId = nextId.fetch_add(1, std::memory_order_relaxed);
	m_settings.noiseSampleStep = settings.noiseSampleStep >= 4 ? 4
		: settings.noiseSampleStep >= 2 ? 2 : 1;
	m_noise.reserve(FieldCount);
	for (unsigned int i = 0; i < FieldCount; ++i)
		m_noise.emplace_back(static_cast<unsigned int>(mix(seed + i)));
}

float CaveGenerator::interpolatedSample(Field field, float x, float y, float z, float xzScale, float yScale) const {
	const int step = m_settings.noiseSampleStep;
	int slot;
	switch (field) {
		case Cheese: slot = 0; break;
		case Region: slot = 1; break;
		case Thickness: slot = 2; break;
		case NoodleRegion: slot = 3; break;
		default: return rawSample(field, x, y, z, xzScale, yScale);
	}
	// Settings normalize to powers of two; avoid runtime float/integer divides
	// in the per-voxel path.
	const float inverseStep = step == 4 ? 0.25f : 0.5f;
	const int cy = int(std::floor(y*inverseStep));
	struct Segment {
		uint64_t owner = 0;
		float x = 0, z = 0, low = 0, high = 0;
		int y = 0;
	};
	static thread_local std::array<Segment,4> segments;
	auto& segment = segments[slot];
	if (segment.owner == m_cacheId && segment.x == x && segment.z == z && segment.y == cy)
		return segment.low + (y-float(cy)*step)*inverseStep*(segment.high-segment.low);
	// Share lattice vertices inside 32-block tiles. Neighboring cells reuse
	// their corners; caching eight independently evaluated corners per cell
	// can cost more noise calls than exact sampling, especially at step 2.
	struct Tile {
		uint64_t owner = 0;
		int x = 0, y = 0, z = 0;
		std::vector<float> values;
		std::vector<uint64_t> valid;
	};
	struct Cache {
		std::array<Tile,8> tiles;
		size_t last = 0, next = 0;
	};
	static thread_local Cache cache;
	const int cells = step == 4 ? 8 : 16, side = cells+1;
	const size_t nodes = size_t(side)*side*side, words = (nodes+63)/64;
	int cx = int(std::floor(x*inverseStep)), cz = int(std::floor(z*inverseStep));
	int tileX = int(std::floor(x/32.0f)), tileY = int(std::floor(y/32.0f)), tileZ = int(std::floor(z/32.0f));
	auto matches = [&](const Tile& t) {
		return t.owner == m_cacheId && t.x == tileX && t.y == tileY && t.z == tileZ;
	};
	if (!matches(cache.tiles[cache.last])) {
		size_t found = 0;
		while (found < cache.tiles.size() && !matches(cache.tiles[found])) ++found;
		if (found == cache.tiles.size()) {
			found = cache.next;
			cache.next = (cache.next+1)%cache.tiles.size();
			auto& t = cache.tiles[found];
			t.owner = m_cacheId; t.x = tileX; t.y = tileY; t.z = tileZ;
			t.values.resize(4*nodes);
			t.valid.assign(4*words,0);
		}
		cache.last = found;
	}
	auto& tile = cache.tiles[cache.last];
	const int lx = cx-tileX*cells, ly = cy-tileY*cells, lz = cz-tileZ*cells;
	auto lerp = [](float a, float b, float t) { return a + t*(b-a); };
	float tx = (x-float(cx)*step)*inverseStep, tz = (z-float(cz)*step)*inverseStep;
	auto slice = [&](int dy) {
		auto vertex = [&](int dx, int dz) {
			size_t node = size_t(lx+dx) + size_t(side)*(size_t(ly+dy) + size_t(side)*(lz+dz));
			auto& bits = tile.valid[slot*words+node/64];
			uint64_t bit = uint64_t(1) << (node%64);
			auto& value = tile.values[slot*nodes+node];
			if (!(bits & bit)) {
				value = rawSample(field,float(cx)*step+dx*step,float(cy)*step+dy*step,
					float(cz)*step+dz*step,xzScale,yScale);
				bits |= bit;
			}
			return value;
		};
		float near = lerp(vertex(0,0),vertex(1,0),tx);
		float far = lerp(vertex(0,1),vertex(1,1),tx);
		return lerp(near,far,tz);
	};
	bool sameColumn = segment.owner == m_cacheId && segment.x == x && segment.z == z;
	// Adjacent vertical segments share a whole interpolated endpoint too.
	float low = sameColumn && segment.y+1 == cy ? segment.high : slice(0);
	float high = sameColumn && segment.y-1 == cy ? segment.low : slice(1);
	// Interpolate signed fields before thresholding, never a stone/air decision.
	segment = {m_cacheId,x,z,low,high,cy};
	return lerp(low,high,(y-float(cy)*step)*inverseStep);
}

CaveGenerator::Column CaveGenerator::prepareColumn(int x, int z, int surfaceY) const {
	const auto& s = m_settings;
	float entrance = surfaceY >= s.entranceMinSurfaceY
		? smooth(s.entranceThreshold, s.entranceThreshold + s.entranceRegionFade,
			sample(Entrance, x, 0, z, s.entranceScale, 0)) : 0;
	float pillar = tunnelDistance(PillarA, PillarB, x, 0, z, s.pillarScale, 0);
	return {x, z, surfaceY, entrance, pillar};
}

float CaveGenerator::tunnelDistance(Field a, Field b, float x, float y, float z,
	float xzScale, float yScale) const {
	// Each zero set is a surface. Two independent surfaces intersect along
	// winding curves; thickening their intersection produces tubes, not blobs.
	return std::max(std::abs(sample(a, x, y, z, xzScale, yScale)),
		std::abs(sample(b, x, y, z, xzScale, yScale)));
}

bool CaveGenerator::isCheese(const Column& column, float x, float y, float z,
	float protection, float depth) const {
	const auto& s = m_settings;
	float base = sample(Cheese, x, y, z, s.cheeseScaleXZ, s.cheeseScaleY);
	// Each gradient uses two offsets of magnitude <=1, and interpolation
	// is convex. Use the conservative bound 2, including custom frequencies.
	float minimumThreshold = s.cheeseThreshold - 2*std::abs(s.cheeseThresholdVariation)
		- s.cheeseDepthBonus * depth + (1 - protection);
	if (base + s.cheeseDetailStrength <= minimumThreshold) return false;
	float threshold = s.cheeseThreshold + s.cheeseThresholdVariation *
		sample(Region, x, y, z, s.cheeseRegionScale, s.cheeseRegionScale)
		- s.cheeseDepthBonus * depth + (1 - protection);
	if (base + s.cheeseDetailStrength <= threshold) return false;
	float field = base + s.cheeseDetailStrength *
		sample(Detail, x, y, z, s.cheeseDetailScale, s.cheeseDetailScale);
	if (field <= threshold) return false;
	// Two 2D zero sets leave scattered columns, rather than long stone walls.
	// Slowly changing radius and the chamber boundary blend them into the rock.
	float radius = s.pillarThickness + s.pillarVariation *
		sample(PillarY, x, y, z, s.pillarScale, s.pillarYScale);
	return column.pillar >= radius;
}

CaveKind CaveGenerator::classify(const Column& column, int y) const {
	const auto& s = m_settings;
	if (y <= s.minY || y >= s.maxY || y > column.surfaceY) return CaveKind::Solid;
	float depth = column.surfaceY - y;
	float boundary = smooth(s.minY, s.minY + s.bottomProtectionHeight, y) *
		(1 - smooth(s.maxY - s.topProtectionHeight, s.maxY, y));
	float surface = smooth(2, s.surfaceProtectionDepth, depth);
	bool entrance = column.entrance > 0 && depth < s.entranceMaxDepth;
	if (surface == 0 && !entrance) return CaveKind::Solid;
	float x = column.x, wy = y, z = column.z;
	if (s.warpStrength != 0) {
		x += s.warpStrength * sample(WarpX, column.x, y, column.z, s.warpScale, s.warpScale);
		wy += s.warpStrength * sample(WarpY, column.x, y, column.z, s.warpScale, s.warpScale);
		z += s.warpStrength * sample(WarpZ, column.x, y, column.z, s.warpScale, s.warpScale);
	}
	float deep = smooth(s.surfaceProtectionDepth, s.deepCaveDepth, depth);
	float protection = surface * boundary;
	// Entrances reuse the backbone's curves so their lower ends overlap real
	// tunnels. The regional gate fades away as ordinary surface protection lifts.
	float distance = tunnelDistance(SpaghettiA, SpaghettiB, x, wy, z,
		s.spaghettiScaleXZ, s.spaghettiScaleY);
	if (entrance && distance < s.entranceThickness * column.entrance * boundary *
		(1 - smooth(s.surfaceProtectionDepth, s.entranceMaxDepth, depth)))
		return CaveKind::Entrance;
	if (protection == 0) return CaveKind::Solid;
	float maximumWidth = s.spaghettiThickness + s.spaghettiDepthBonus * deep + 2*std::abs(s.spaghettiThicknessVariation);
	if (distance < (maximumWidth + s.spaghettiRoughnessStrength) * protection) {
		float width = s.spaghettiThickness + s.spaghettiDepthBonus * deep +
			s.spaghettiThicknessVariation * sample(Thickness, x, wy, z, s.thicknessScale, s.thicknessScale);
		if (distance < (width + s.spaghettiRoughnessStrength) * protection) {
			float roughness = sample(Roughness, x, wy, z, s.spaghettiRoughnessScale, s.spaghettiRoughnessScale);
			if (distance < (width - roughness * s.spaghettiRoughnessStrength) * protection)
				return CaveKind::Spaghetti;
		}
	}
	if (isCheese(column, x, wy, z, protection, deep)) return CaveKind::Cheese;
	float mask = smooth(s.noodleRegionThreshold, s.noodleRegionThreshold + s.noodleRegionFade,
		sample(NoodleRegion, x, wy, z, s.noodleRegionScale, s.noodleRegionScale));
	if (mask > 0 && tunnelDistance(NoodleA, NoodleB, x, wy, z, s.noodleScaleXZ, s.noodleScaleY)
		< s.noodleThickness * mask * protection) return CaveKind::Noodle;
	return CaveKind::Solid;
}
