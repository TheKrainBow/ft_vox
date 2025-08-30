#include "CaveGenerator.hpp"

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
	m_perlin(seed) {}

// bool CaveGenerator::isAir(int x, int y, int z) const
// {
// 	// Surface height from 2D slice of Perlin
// 	float hNoise = m_perlin.noise(x * m_surfaceScale, 0.0f, z * m_surfaceScale);
// 	int surfaceHeight = static_cast<int>((hNoise * 0.5f + 0.5f) * (m_worldHeight - 1));

// 	if (y > surfaceHeight) return true; // Above terrain

// 	// 3D fractal Perlin for caves
// 	float caveNoise = m_perlin.fractalNoise(
// 		x * m_caveScale,
// 		y * m_caveScale,
// 		z * m_caveScale,
// 		1, // Octaves
// 		2.0f, // Lacunarity
// 		0.5f // Persistance
// 	);

// 	// Map [-1,1] -> [0,1]
// 	caveNoise = (caveNoise * 0.5f + 0.5f);

// 	// Vertical bias
// 	float bias = (float)y / (float)m_worldHeight;
// 	bias *= m_verticalBiasStrength;

// 	return (caveNoise + bias) > m_caveThreshold;
// }

// bool CaveGenerator::isAir(int x, int y, int z) const {
// 	// Surface height (use 2D slice of Perlin)
// 	float hNoise = m_perlin.noise(x * m_surfaceScale, 0.0f, z * m_surfaceScale);
// 	int surfaceHeight = static_cast<int>((hNoise * 0.5f + 0.5f) * (m_worldHeight - 1));

// 	if (y > surfaceHeight) return true; // Above terrain

// 	// Cave noise
// 	float caveNoise = m_perlin.noise(x * m_caveScale, y * m_caveScale, z * m_caveScale);
// 	caveNoise = (caveNoise * 0.5f + 0.5f); // normalize to [0,1]

// 	float bias = static_cast<float>(y) / static_cast<float>(m_worldHeight);
// 	bias *= m_verticalBiasStrength;

// 	return (caveNoise + bias) > m_caveThreshold;
// }

bool CaveGenerator::isAir(int x, int y, int z) const {
	// Surface
	float hNoise = m_perlin.noise(x * m_surfaceScale, 0.0f, z * m_surfaceScale);
	int surfaceHeight = (int)((hNoise * 0.5f + 0.5f) * (m_worldHeight - 1));

	if (y > surfaceHeight) return true;

	int depth = surfaceHeight - y;

	// Block caves right at the surface
	const int caveSurfaceBuffer = 5; 
	if (depth < caveSurfaceBuffer) return false;

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
	entranceNoise = (entranceNoise * 0.5f + 0.5f);

	bool allowEntrance = entranceNoise > 0.75f; // only some columns get entrances

	// Depth fade
	float depthFactor = std::clamp((float)depth / 15.0f, 0.0f, 1.0f);

	// Bias with height
	float bias = (float)y / (float)m_worldHeight;
	bias *= m_verticalBiasStrength;

	// Apply entrance restriction
	if (depth < 20 && !allowEntrance) {
		return false; // force stone near surface unless marked entrance
	}

	return (caveNoise * depthFactor + bias) > m_caveThreshold;
}