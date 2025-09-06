#include "Noise3DGenerator.hpp"

Noise3DGenerator::Noise3DGenerator(unsigned int seed = 42) {
	// Initialize permutation vector with 0..255
	p.resize(256);
	std::iota(p.begin(), p.end(), 0);

	// Shuffle using given seed
	std::default_random_engine engine(seed);
	std::shuffle(p.begin(), p.end(), engine);

	// Duplicate the permutation vector
	p.insert(p.end(), p.begin(), p.end());
}

// Fractal noise: sum of multiple octaves
float Noise3DGenerator::fractalNoise(float x, float y, float z,
			int octaves = 4,
			float lacunarity = 2.0f,
			float persistence = 0.5f) const
{
	float amplitude = 1.0f;
	float frequency = 1.0f;
	float sum = 0.0f;
	float maxSum = 0.0f;

	for (int i = 0; i < octaves; i++) {
	sum += amplitude * noise(x * frequency, y * frequency, z * frequency);
	maxSum += amplitude;
	amplitude *= persistence;
	frequency *= lacunarity;
	}

	// Normalize to [-1,1]
	return sum / maxSum;
}

/// Returns noise in range [-1,1]
float Noise3DGenerator::noise(float x, float y, float z) const {
	// Find unit cube containing point
	int X = static_cast<int>(std::floor(x)) & 255;
	int Y = static_cast<int>(std::floor(y)) & 255;
	int Z = static_cast<int>(std::floor(z)) & 255;

	// Relative x, y, z of point in cube
	x -= std::floor(x);
	y -= std::floor(y);
	z -= std::floor(z);

	// Compute fade curves
	float u = fade(x);
	float v = fade(y);
	float w = fade(z);

	// Hash coordinates of the 8 cube corners
	int A  = p[X] + Y,	AA = p[A] + Z,	AB = p[A + 1] + Z;
	int B  = p[X + 1] + Y, BA = p[B] + Z,   BB = p[B + 1] + Z;

	// Add blended results from 8 corners of cube
	float res = lerp(w,
		lerp(v,
			lerp(u, grad(p[AA], x, y, z),
					grad(p[BA], x - 1, y, z)),
			lerp(u, grad(p[AB], x, y - 1, z),
					grad(p[BB], x - 1, y - 1, z))),
		lerp(v,
			lerp(u, grad(p[AA + 1], x, y, z - 1),
					grad(p[BA + 1], x - 1, y, z - 1)),
			lerp(u, grad(p[AB + 1], x, y - 1, z - 1),
					grad(p[BB + 1], x - 1, y - 1, z - 1)))
	);

	return res; // already in [-1,1]
}

float Noise3DGenerator::fade(float t) {
	// 6t^5 - 15t^4 + 10t^3
	return t * t * t * (t * (t * 6 - 15) + 10);
}

float Noise3DGenerator::lerp(float t, float a, float b) {
	return a + t * (b - a);
}

float Noise3DGenerator::grad(int hash, float x, float y, float z) {
	int h = hash & 15;
	float u = h < 8 ? x : y;
	float v = h < 4 ? y : (h == 12 || h == 14 ? x : z);
	return ((h & 1) ? -u : u) + ((h & 2) ? -v : v);
}
