#pragma once
#include "ft_vox.hpp"

struct CaveNoiseData {
	double amplitude;
	double frequency;
	double persistence;
	double lacunarity;
	size_t nb_octaves;
};

class CaveGenerator {
public:
	CaveGenerator(size_t seed);
	~CaveGenerator() = default;

	void setSeed(size_t seed);
	void setNoiseData(const CaveNoiseData& data);
	double getCaveNoise(ivec3 pos);
	bool isCave(ivec3 pos, double threshold = 0.0);
	double noise(double x, double y, double z) const;

private:
	size_t _seed;
	CaveNoiseData _data;
	std::vector<int> _permutation;

	double singleNoise(double x, double y, double z) const;
	double fade(double t) const;
	double lerp(double a, double b, double t) const;
	double grad(int hash, double x, double y, double z) const;
};
