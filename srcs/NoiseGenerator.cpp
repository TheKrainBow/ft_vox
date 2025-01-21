/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   NoiseGenerator.cpp                                 :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: maagosti <maagosti@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/01/02 22:51:33 by tmoragli          #+#    #+#             */
/*   Updated: 2025/01/21 01:01:07 by maagosti         ###   ########.fr       */
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
	clearPerlinMaps();
}

void NoiseGenerator::setSeed(size_t seed)
{
	std::cout << "Setting seed to: " << seed << std::endl;
	_seed = seed;
}

void NoiseGenerator::clearPerlinMaps(void)
{
	for (auto &map : _perlinMaps)
	{
		if (map && map->map)
			delete [] map->map;
		map->map = nullptr;
		if (map)
			delete map;
		map = nullptr;
	}
	_perlinMaps.clear();
}

NoiseGenerator::PerlinMap *NoiseGenerator::addPerlinMap(int startX, int startZ, int size, int resolution)
{
	NoiseGenerator::PerlinMap *newMap = new NoiseGenerator::PerlinMap();
	newMap->size = size;
	newMap->map = new double[size * size];
	newMap->resolution = resolution;
	newMap->position = vec2(startX, startZ);
	for (int x = 0; x < size; x += resolution)
		for (int z = 0; z < size; z += resolution)
		{
			newMap->map[z * size + x] = noise((startX * size) + x, (startZ * size) + z);
			newMap->map[z * size + x] = 100.0 + (newMap->map[z * size + x] * 100.0);
			newMap->map[z * size + x] = clamp(newMap->map[z * size + x], 0.0, 255.0);
			if (newMap->heighest == std::numeric_limits<double>::min() || newMap->map[z * size + x] > newMap->heighest)
				newMap->heighest = newMap->map[z * size + x];
		}
	_perlinMaps.push_back(newMap);
	return (newMap);
}

NoiseGenerator::PerlinMap *NoiseGenerator::getPerlinMap(int x, int y)
{
	for (auto &map : _perlinMaps)
	{
		if (map->position.x == x && map->position.y == y)
			return (map);
	}
	return nullptr;
}

// Layered perlin noise samples by octaves number
double NoiseGenerator::noise(double x, double y) const
{
	double total = 0.0;
	double frequency = 0.01;
	double amplitude = 1.0;
	double maxValue = 0.0; // Used for normalizing

	for (size_t i = 0; i < _nb_octaves; i++) {
		total += singleNoise(x * frequency, y * frequency) * amplitude;
		maxValue += amplitude;

		amplitude *= _persistance;
		frequency *= _lacunarity;
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
