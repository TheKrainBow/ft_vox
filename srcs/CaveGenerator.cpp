#include "CaveGenerator.hpp"

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
	m_noise.reserve(FieldCount);
	for (unsigned int i = 0; i < FieldCount; ++i)
		m_noise.emplace_back(static_cast<unsigned int>(mix(seed + i)));
}

float CaveGenerator::sample(Field field, float x, float y, float z, float xzScale, float yScale) const {
	// Fractional, field-specific phases avoid all Perlin fields vanishing at
	// shared lattice points (especially the world origin).
	float phase = static_cast<int>(field) * 13.371f;
	return m_noise[field].noise(x * xzScale + phase + 0.31f,
		y * yScale + phase + 0.73f, z * xzScale + phase + 0.19f);
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
	float threshold = s.cheeseThreshold + s.cheeseThresholdVariation *
		sample(Region, x, y, z, s.cheeseRegionScale, s.cheeseRegionScale)
		- s.cheeseDepthBonus * depth + (1 - protection);
	float base = sample(Cheese, x, y, z, s.cheeseScaleXZ, s.cheeseScaleY);
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
	float width = s.spaghettiThickness + s.spaghettiDepthBonus * deep +
		s.spaghettiThicknessVariation * sample(Thickness, x, wy, z, s.thicknessScale, s.thicknessScale);
	if (distance < (width + s.spaghettiRoughnessStrength) * protection) {
		float roughness = sample(Roughness, x, wy, z, s.spaghettiRoughnessScale, s.spaghettiRoughnessScale);
		if (distance < (width - roughness * s.spaghettiRoughnessStrength) * protection)
			return CaveKind::Spaghetti;
	}
	if (isCheese(column, x, wy, z, protection, deep)) return CaveKind::Cheese;
	float mask = smooth(s.noodleRegionThreshold, s.noodleRegionThreshold + s.noodleRegionFade,
		sample(NoodleRegion, x, wy, z, s.noodleRegionScale, s.noodleRegionScale));
	if (mask > 0 && tunnelDistance(NoodleA, NoodleB, x, wy, z, s.noodleScaleXZ, s.noodleScaleY)
		< s.noodleThickness * mask * protection) return CaveKind::Noodle;
	return CaveKind::Solid;
}
