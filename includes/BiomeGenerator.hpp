/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   BiomeGenerator.hpp                                 :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: tmoragli <tmoragli@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/01/07 23:34:45 by tmoragli          #+#    #+#             */
/*   Updated: 2025/04/13 23:08:44 by tmoragli         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once

#include "NoiseGenerator.hpp"
#include "SubChunk.hpp"

#define BIOME_SIZE 500.0
#define OFFSET_METRIC 1000.0

const glm::vec2 directions[8] = {
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
	glm::vec2 center = {0.0f, 0.0f};
	BiomeType type = DEFAULT;
};

class BiomeGenerator
{
	public:
		BiomeGenerator(size_t seed);
		~BiomeGenerator();
		void findBiomeCenters(glm::vec2 playerPos);
		void showBiomeCenters() const;
		BiomeType findClosestBiomes(double x, double y) const;
	private:
		std::vector<BiomeData> _biomes;
		std::unordered_set<glm::vec2, vec2_hash> _biomeCentersTemp;
		size_t _seed;
		NoiseGenerator _noiseGen;
};