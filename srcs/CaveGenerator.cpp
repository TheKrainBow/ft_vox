#include "CaveGenerator.hpp"

CaveGenerator::CaveGenerator(size_t seed) : _seed(seed) {
	std::mt19937 generator(seed);
	std::vector<int> p(256);
	for (int i = 0; i < 256; ++i) p[i] = i;
	std::shuffle(p.begin(), p.end(), generator);
	_permutation.resize(512);
	for (int i = 0; i < 512; ++i) _permutation[i] = p[i % 256];

	// Default noise data
	_data = {1.0, 0.03, 0.5, 2.0, 5};
}

void CaveGenerator::setSeed(size_t seed) {
	_seed = seed;
	std::mt19937 generator(seed);
	std::vector<int> p(256);
	for (int i = 0; i < 256; ++i) p[i] = i;
	std::shuffle(p.begin(), p.end(), generator);
	for (int i = 0; i < 512; ++i) _permutation[i] = p[i % 256];
}

void CaveGenerator::setNoiseData(const CaveNoiseData& data) {
	_data = data;
}

double CaveGenerator::getCaveNoise(ivec3 pos) {
	return noise(pos.x, pos.y, pos.z);
}

bool CaveGenerator::isCave(ivec3 pos, double threshold) {
	return getCaveNoise(pos) < threshold;
}

double CaveGenerator::noise(double x, double y, double z) const {
	double total = 0.0;
	double amplitude = _data.amplitude;
	double frequency = _data.frequency;
	double maxValue = 0.0;

	for (size_t i = 0; i < _data.nb_octaves; ++i) {
		total += singleNoise(x * frequency, y * frequency, z * frequency) * amplitude;
		maxValue += amplitude;
		amplitude *= _data.persistence;
		frequency *= _data.lacunarity;
	}
	return total / maxValue;
}

double CaveGenerator::singleNoise(double x, double y, double z) const {
	int X = (int)floor(x) & 255;
	int Y = (int)floor(y) & 255;
	int Z = (int)floor(z) & 255;

	x -= floor(x);
	y -= floor(y);
	z -= floor(z);

	double u = fade(x);
	double v = fade(y);
	double w = fade(z);

	int A = _permutation[X] + Y;
	int AA = _permutation[A] + Z;
	int AB = _permutation[A + 1] + Z;
	int B = _permutation[X + 1] + Y;
	int BA = _permutation[B] + Z;
	int BB = _permutation[B + 1] + Z;

	return lerp(w,
		lerp(v,
			lerp(u, grad(_permutation[AA], x, y, z), grad(_permutation[BA], x - 1, y, z)),
			lerp(u, grad(_permutation[AB], x, y - 1, z), grad(_permutation[BB], x - 1, y - 1, z))
		),
		lerp(v,
			lerp(u, grad(_permutation[AA + 1], x, y, z - 1), grad(_permutation[BA + 1], x - 1, y, z - 1)),
			lerp(u, grad(_permutation[AB + 1], x, y - 1, z - 1), grad(_permutation[BB + 1], x - 1, y - 1, z - 1))
		)
	);
}

double CaveGenerator::fade(double t) const {
	return t * t * t * (t * (t * 6 - 15) + 10);
}

double CaveGenerator::lerp(double a, double b, double t) const {
	return a + t * (b - a);
}

double CaveGenerator::grad(int hash, double x, double y, double z) const {
	int h = hash & 15;
	double u = h < 8 ? x : y;
	double v = h < 4 ? y : (h == 12 || h == 14 ? x : z);
	return ((h & 1) == 0 ? u : -u) + ((h & 2) == 0 ? v : -v);
}
