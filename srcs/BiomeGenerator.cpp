/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   BiomeGenerator.cpp                                 :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: tmoragli <tmoragli@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/01/07 23:55:27 by tmoragli          #+#    #+#             */
/*   Updated: 2025/04/15 10:45:55 by tmoragli         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "BiomeGenerator.hpp"

BiomeGenerator::BiomeGenerator(size_t seed) : _seed(seed), _noiseGen(seed)
{
	_noiseGen.setNoiseData({ 1.0, 0.1, 0.5, 5.0, 4 });
}

BiomeGenerator::~BiomeGenerator()
{
}

void BiomeGenerator::findBiomeCenters(ivec2 playerPos)
{
	_biomes.clear();
	ivec2 pos {0.0f, 0.0f};

	pos.x = floor(playerPos.x / BIOME_SIZE) * BIOME_SIZE;
	pos.y = floor(playerPos.y / BIOME_SIZE) * BIOME_SIZE;

	if (pos.x == 0)
		pos.x = BIOME_SIZE;
	if (pos.y == 0)
		pos.y = BIOME_SIZE;

	// Generate raw centers
	for (const ivec2 &direction : directions)
	{
		ivec2 newBiomePoint_1 = ivec2(pos.x + direction.x * BIOME_SIZE, pos.y + direction.y * BIOME_SIZE);
		ivec2 newBiomePoint_2 = ivec2(pos.x + direction.x * BIOME_SIZE, pos.y + direction.y * BIOME_SIZE * 2);
		ivec2 newBiomePoint_3 = ivec2(pos.x - direction.x * 2 * BIOME_SIZE, pos.y - direction.y * BIOME_SIZE);
		ivec2 newBiomePoint_4 = ivec2(pos.x - direction.x * 2 * BIOME_SIZE, pos.y - direction.y * BIOME_SIZE * 2);

		_biomeCentersTemp.emplace(newBiomePoint_1.x, newBiomePoint_1.y);
		_biomeCentersTemp.emplace(newBiomePoint_2.x, newBiomePoint_2.y);
		_biomeCentersTemp.emplace(newBiomePoint_3.x, newBiomePoint_3.y);
		_biomeCentersTemp.emplace(newBiomePoint_4.x, newBiomePoint_4.y);
	}

	// Add offset and calculate temps
	for (const ivec2 &vec : _biomeCentersTemp)
	{
		BiomeData data;

		// Create a noise data depending on the position that will serve for temps and offset
		double noise = _noiseGen.noise(vec.x * 0.01, vec.y * 0.01) * 10.0;
		if (noise > 1.0)
			noise *= 0.1;
		else if (noise < -1.0)
			noise *= 0.1;

		// Apply offset and amplify with an arbitrary but constant metric
		data.center.x = vec.x + (noise * OFFSET_METRIC);
		data.center.y = vec.y + (noise * OFFSET_METRIC);

		// Determine biome type with noise generated (0.0 < temperature < 1.0)
		double temperature = (noise + 1.0) * 0.5;
		if (temperature <= 0.5)
			data.type = PLAINS;
		else if (temperature > 0.5)
			data.type = DESERT;
		_biomes.push_back(data);
	}
}

BiomeType BiomeGenerator::findClosestBiomes(double x, double y) const
{
	double minDistanceSquared = std::numeric_limits<double>::max();
	BiomeData closestBiome;

	for (const BiomeData &data : _biomes)
	{
		double dx = x - data.center.x;
		double dz = y - data.center.y;
		double distanceSquared = dx * dx + dz * dz;

		if (distanceSquared < minDistanceSquared)
		{
			minDistanceSquared = distanceSquared;
			closestBiome = data;
		}
	}

	return closestBiome.type;
}

void BiomeGenerator::showBiomeCenters() const
{
	for (const BiomeData &data : _biomes)
	{
		ivec2 pos = {data.center.x, data.center.y};

		// Red color
		glColor3f(1.0f, 0.0f, 0.0f);
		glBegin(GL_LINES);

		// Vertical lines for each center
		glVertex3f(pos.x, 0.0f, pos.y);
		glVertex3f(pos.x, 255.0f, pos.y);
		glEnd();
		glColor3d(1.0f, 1.0f, 1.0f);
	}
}
