#pragma once

#include "Noise3DGenerator.hpp"
#include <cstdint>

// Frequencies are inverse blocks; thresholds use signed Perlin noise [-1, 1].
struct CaveGenerationSettings {
	int minY = 1, maxY = 640; // Bedrock is y=0; current surface peaks reach ~630.
	float bottomProtectionHeight = 10, topProtectionHeight = 16;
	float surfaceProtectionDepth = 14, deepCaveDepth = 75;
	float cheeseScaleXZ = 0.032f, cheeseScaleY = 0.052f;
	float cheeseDetailScale = 0.075f, cheeseDetailStrength = 0.15f;
	float cheeseThreshold = 0.53f, cheeseThresholdVariation = 0.22f;
	float cheeseRegionScale = 0.006f, cheeseDepthBonus = 0.09f;
	float spaghettiScaleXZ = 0.032f, spaghettiScaleY = 0.042f;
	float spaghettiThickness = 0.105f, spaghettiThicknessVariation = 0.045f;
	float thicknessScale = 0.009f, spaghettiDepthBonus = 0.025f;
	float spaghettiRoughnessScale = 0.09f, spaghettiRoughnessStrength = 0.012f;
	float noodleScaleXZ = 0.065f, noodleScaleY = 0.075f;
	float noodleThickness = 0.070f, noodleRegionScale = 0.012f;
	float noodleRegionThreshold = 0.18f, noodleRegionFade = 0.18f;
	float entranceScale = 0.014f, entranceThreshold = 0.43f;
	float entranceRegionFade = 0.15f, entranceMaxDepth = 26;
	float entranceThickness = 0.13f;
	int entranceMinSurfaceY = 115; // Keep existing oceans/beaches sealed (sea level 111).
	float pillarScale = 0.045f, pillarThickness = 0.085f;
	float pillarYScale = 0.025f, pillarVariation = 0.025f;
	float warpScale = 0.012f, warpStrength = 0; // Optional; three extra samples.
};

enum class CaveKind : uint8_t { Solid, Cheese, Spaghetti, Noodle, Entrance };

class CaveGenerator {
public:
	explicit CaveGenerator(uint64_t seed, CaveGenerationSettings settings = {});
	struct Column {
		int x, z, surfaceY;
		float entrance, pillar;
	};
	Column prepareColumn(int x, int z, int surfaceY) const;
	CaveKind classify(const Column& column, int y) const;
	bool isAir(int x, int y, int z, int surfaceY) const {
		return classify(prepareColumn(x, z, surfaceY), y) != CaveKind::Solid;
	}
private:
	enum Field { Cheese, Detail, Region, SpaghettiA, SpaghettiB, Thickness,
		Roughness, NoodleA, NoodleB, NoodleRegion, Entrance, PillarA, PillarB,
		PillarY, WarpX, WarpY, WarpZ, FieldCount };
	CaveGenerationSettings m_settings;
	std::vector<Noise3DGenerator> m_noise;
	float sample(Field field, float x, float y, float z, float xzScale, float yScale) const;
	float tunnelDistance(Field a, Field b, float x, float y, float z, float xzScale, float yScale) const;
	bool isCheese(const Column& column, float x, float y, float z, float protection, float depth) const;
};
