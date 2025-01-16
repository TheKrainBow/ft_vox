#pragma once

#include "blocks/ABlock.hpp"
#include "blocks/Dirt.hpp"
#include "blocks/Cobble.hpp"
#include "blocks/Grass.hpp"
#include "blocks/Stone.hpp"
#include "NoiseGenerator.hpp"
#include "ft_vox.hpp"

#define CHUNK_SIZEV2 16

class World;

class ChunkV2
{
	private:
		vec3	_position;
		char	_blocks[CHUNK_SIZEV2 * CHUNK_SIZEV2 * CHUNK_SIZEV2];
		double	_perlinMap[CHUNK_SIZEV2 * CHUNK_SIZEV2];
		World	&_world;
	public:
		ChunkV2(int x, int y, int z, World &world);
		~ChunkV2();
		void sendFacesToDisplay();
		void load();
		void positiveLoad();
		void negativeLoad();
		vec3 getPosition(void);
	    char getBlock(int x, int y, int z);
		// void renderBoundaries() const;
	private:
		void addDirtBlock(int x, int y, int z);
		void addStoneBlock(int x, int y, int z);
		void addGrassBlock(int x, int y, int z);
};
