#pragma once
#include <vector>
#include <numeric>
#include <random>
#include <algorithm>
#include <cmath>

class Noise3DGenerator {
	public:
		Noise3DGenerator(unsigned int seed);
		float noise(float x, float y, float z) const;

	private:
		std::vector<int> p;
		static float fade(float t);
		static float lerp(float t, float a, float b);
		static float grad(int hash, float x, float y, float z);
};
