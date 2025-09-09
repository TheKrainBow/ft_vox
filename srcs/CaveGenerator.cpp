#include "CaveGenerator.hpp"
#include <iostream>
CaveGenerator::CaveGenerator(int worldHeight,
		float surfaceScale,
		float caveScale,
		float caveThreshold,
		float verticalBiasStrength,
		unsigned int seed = 42)
	: m_worldHeight(worldHeight),
	m_surfaceScale(surfaceScale),
	m_caveScale(caveScale),
	m_caveThreshold(caveThreshold),
	m_verticalBiasStrength(verticalBiasStrength),
	m_perlin(seed) {
	}

bool CaveGenerator::isAir(int x, int y, int z, int currentHeight) const {
	// Surface
	float hNoise = m_perlin.noise(x * m_surfaceScale, 0.0f, z * m_surfaceScale);
	int surfaceHeight = (int)((hNoise * 0.5f + 0.5f) * (currentHeight - 1));

	if (y > surfaceHeight) return false;

	int depth = surfaceHeight - y;

	// Block caves right at the surface
	const int caveSurfaceBuffer = 5; 
	if (depth < caveSurfaceBuffer) return true;

	// Main cave noise
	float caveNoise = m_perlin.fractalNoise(
		x * m_caveScale,
		y * m_caveScale,
		z * m_caveScale,
		1,
		2.0,
		0.5
	);
	caveNoise = (caveNoise * 0.5f + 0.5f);

	// Entrance control (2D noise)
	float entranceNoise = m_perlin.fractalNoise(
		x * 0.01f, 0.0f, z * 0.01f, 3, 2.0f, 0.5f
	);
	entranceNoise = m_perlin.fractalNoise(x*0.005f, 0.0f, z*0.005f, 3, 2.0f, 0.5f);
	bool allowEntrance = entranceNoise > 0.85f;
	// entranceNoise = (entranceNoise * 0.5f + 0.5f);

	// bool allowEntrance = entranceNoise > 0.75f; // only some columns get entrances

	// Depth fade
	float depthFactor = std::clamp((float)depth / 15.0f, 0.0f, 1.0f);

	// Bias with height
	float bias = (float)y / (float)currentHeight;
	bias *= m_verticalBiasStrength;

	// Apply entrance restriction
	if (depth < 20 && !allowEntrance) {
		return true; // force stone near surface unless marked entrance
	}

	return (caveNoise * depthFactor + bias) <= m_caveThreshold;
}
