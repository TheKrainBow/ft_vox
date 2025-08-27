#include "Noise3DGenerator.hpp"

class CaveGenerator {
	public:
		CaveGenerator(int worldHeight,
						float surfaceScale,
						float caveScale,
						float caveThreshold,
						float verticalBiasStrength,
						unsigned int seed);

		bool isAir(int x, int y, int z) const;

	private:
		int m_worldHeight;
		float m_surfaceScale;
		float m_caveScale;
		float m_caveThreshold;
		float m_verticalBiasStrength;
		Noise3DGenerator m_perlin;
};
