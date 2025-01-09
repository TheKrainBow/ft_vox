/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   NoiseGenerator.cpp                                 :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: tmoragli <tmoragli@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/01/02 22:51:33 by tmoragli          #+#    #+#             */
/*   Updated: 2025/01/09 01:08:14 by tmoragli         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "NoiseGenerator.hpp"

NoiseGenerator::NoiseGenerator(size_t seed): _seed(seed)
{
	std::cout << "The seed for this world is: " << seed << std::endl;

	// Initialize permutation table
	std::mt19937 generator(seed);
	std::vector<int> p(256);
	for (int i = 0; i < 256; i++) p[i] = i;
	std::shuffle(p.begin(), p.end(), generator);
	_permutation.resize(512);
	for (int i = 0; i < 512; i++) _permutation[i] = p[i % 256];
}

NoiseGenerator::~NoiseGenerator()
{

}

void NoiseGenerator::setSeed(size_t seed)
{
	std::cout << "Setting seed to: " << seed << std::endl;
	_seed = seed;
}

const size_t &NoiseGenerator::getSeed() const
{
	return _seed;
}

void NoiseGenerator::setNoiseData(const NoiseData &data)
{
	_data.amplitude = data.amplitude;
	_data.frequency = data.frequency;
	_data.nb_octaves = data.nb_octaves;
	_data.lacunarity = data.lacunarity;
	_data.persistance = data.persistance;
}

// Layered perlin noise samples by octaves number
double NoiseGenerator::noise(double x, double y) const
{
	double total = 0.0;
	double amplitude = _data.amplitude;
	double frequency = _data.frequency;
	double maxValue = 0.0; // Used for normalizing

	for (size_t i = 0; i < _data.nb_octaves; i++) {
		total += singleNoise(x * frequency, y * frequency) * amplitude;
		maxValue += amplitude;

		amplitude *= _data.persistance;
		frequency *= _data.lacunarity;
	}
	return total / maxValue; // Normalize
}

double NoiseGenerator::singleNoise(double x, double y) const
{
	int X = (int)std::floor(x) & 255;
	int Y = (int)std::floor(y) & 255;

	x -= std::floor(x);
	y -= std::floor(y);

	double u = fade(x);
	double v = fade(y);

	int A = _permutation[X] + Y;
	int B = _permutation[X + 1] + Y;

	return lerp(
		lerp(grad(_permutation[A], x, y), grad(_permutation[B], x - 1, y), u),
		lerp(grad(_permutation[A + 1], x, y - 1), grad(_permutation[B + 1], x - 1, y - 1), u),
		v
	);
}

double NoiseGenerator::fade(double t) const
{
	return t * t * t * (t * (t * 6 - 15) + 10);
}

double NoiseGenerator::lerp(double a, double b, double t) const
{
	return a + t * (b - a);
}

double NoiseGenerator::grad(int hash, double x, double y) const
{
	int h = hash & 3; // Only 4 gradients
	double u = h < 2 ? x : y;
	double v = h < 2 ? y : x;
	return ((h & 1) == 0 ? u : -u) + ((h & 2) == 0 ? v : -v);
}
