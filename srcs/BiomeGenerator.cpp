/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   BiomeGenerator.cpp                                 :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: tmoragli <tmoragli@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/01/07 23:55:27 by tmoragli          #+#    #+#             */
/*   Updated: 2025/01/08 01:02:17 by tmoragli         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "BiomeGenerator.hpp"

BiomeGenerator::BiomeGenerator()
{
}

BiomeGenerator::~BiomeGenerator()
{
}

void BiomeGenerator::findBiomeCenters(vec2 playerPos, size_t seed)
{
	_biomeCentersTemp.clear();
	_seed = seed;
	_biomeCentersTemp.emplace(playerPos.x, playerPos.y);
	for (const vec2 &direction : directions)
	{
		vec2 newBiomePoint_1 = {playerPos.x + direction.x * BIOME_LENGTH, playerPos.y + direction.y * BIOME_LENGTH};
		vec2 newBiomePoint_2 = {playerPos.x + direction.x * BIOME_LENGTH, playerPos.y + direction.y * BIOME_LENGTH * 2};
		vec2 newBiomePoint_3 = {playerPos.x - direction.x * 2 * BIOME_LENGTH, playerPos.y - direction.y * BIOME_LENGTH};
		vec2 newBiomePoint_4 = {playerPos.x - direction.x * 2 * BIOME_LENGTH, playerPos.y - direction.y * BIOME_LENGTH * 2};

		_biomeCentersTemp.emplace((int)newBiomePoint_1.x, (int)newBiomePoint_1.y);
		_biomeCentersTemp.emplace((int)newBiomePoint_2.x, (int)newBiomePoint_2.y);
		_biomeCentersTemp.emplace((int)newBiomePoint_3.x, (int)newBiomePoint_3.y);
		_biomeCentersTemp.emplace((int)newBiomePoint_4.x, (int)newBiomePoint_4.y);
	}
}

void BiomeGenerator::showBiomeCenters() const
{
	for (const std::pair<int, int> &center : _biomeCentersTemp)
	{
		vec2 pos = {(float)center.first, (float)center.second};

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
