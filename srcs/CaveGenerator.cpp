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

bool CaveGenerator::isAir(int x, int y, int z) const {
	// Surface height (use 2D slice of Perlin)
	float hNoise = m_perlin.noise(x * m_surfaceScale, 0.0f, z * m_surfaceScale);
	int surfaceHeight = static_cast<int>((hNoise * 0.5f + 0.5f) * (m_worldHeight - 1));

	if (y > surfaceHeight) return true; // Above terrain

	// Cave noise
	float caveNoise = m_perlin.noise(x * m_caveScale, y * m_caveScale, z * m_caveScale);
	caveNoise = (caveNoise * 0.5f + 0.5f); // normalize to [0,1]

	float bias = static_cast<float>(y) / static_cast<float>(m_worldHeight);
	bias *= m_verticalBiasStrength;

	return (caveNoise + bias) > m_caveThreshold;
}
