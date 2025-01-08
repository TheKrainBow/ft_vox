/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   BiomeGenerator.hpp                                 :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: tmoragli <tmoragli@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/01/07 23:34:45 by tmoragli          #+#    #+#             */
/*   Updated: 2025/01/08 01:07:41 by tmoragli         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once

#include "NoiseGenerator.hpp"
#include "Chunk.hpp"

#define BIOME_SIZE 4
#define BIOME_LENGTH CHUNK_SIZE_X * RENDER_DISTANCE

const vec2 directions[8] = {
	{0, 1},
	{1, 1},
	{1, 0},
	{1, -1},
	{0, -1},
	{-1, -1},
	{-1, 0},
	{-1, 1}
};

enum BiomeType
{
	DEFAULT,
	PLAINS,
	DESERT
};

struct BiomeData
{
	vec2 center = {0.0, 0.0};
	vec2 origin  = {0.0, 0.0};;
	BiomeType type = DEFAULT;
};

class BiomeGenerator
{
	public:
		BiomeGenerator();
		~BiomeGenerator();
		void findBiomeCenters(vec2 playerPos, size_t seed);
		void showBiomeCenters() const;
	private:
		std::vector<BiomeData> _biomes;
		std::unordered_set<std::pair<int, int>, pair_hash> _biomeCentersTemp;
		size_t _seed;
};