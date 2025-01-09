/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   BiomeGenerator.hpp                                 :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: tmoragli <tmoragli@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/01/07 23:34:45 by tmoragli          #+#    #+#             */
/*   Updated: 2025/01/09 01:31:38 by tmoragli         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once

#include "NoiseGenerator.hpp"
#include "Chunk.hpp"

#define BIOME_SIZE 500.0
#define OFFSET_METRIC 1000.0

const vec2 directions[8] = {
	{0.0f, 1.0f},
	{1.0f, 1.0f},
	{1.0f, 0.0f},
	{1.0f, -1.0f},
	{0.0f, -1.0f},
	{-1.0f, -1.0f},
	{-1.0f, 0.0f},
	{-1.0f, 1.0f}
};

enum BiomeType
{
	DEFAULT,
	PLAINS,
	DESERT,
	MOUNTAIN
};

struct BiomeData
{
	vec2 center = {0.0f, 0.0f};
	BiomeType type = DEFAULT;
};

class BiomeGenerator
{
	public:
		BiomeGenerator(size_t seed);
		~BiomeGenerator();
		void findBiomeCenters(vec2 playerPos);
		void showBiomeCenters() const;
		BiomeType findClosestBiome(double x, double y) const;
	private:
		std::vector<BiomeData> _biomes;
		std::unordered_set<std::pair<float, float>, pair_hash> _biomeCentersTemp;
		size_t _seed;
		NoiseGenerator _noiseGen;
};