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
		ABlock	*_blocks[CHUNK_SIZEV2 * CHUNK_SIZEV2 * CHUNK_SIZEV2];
		// std::array<ABlock *, CHUNK_SIZEV2 * CHUNK_SIZEV2 * CHUNK_SIZEV2>	_blocks;
		double	_perlinMap[CHUNK_SIZEV2 * CHUNK_SIZEV2];
		bool	_isLoaded = false;
		World	&_world;
	public:
		ChunkV2(int x, int y, int z, World &world);
		~ChunkV2();
		void sendFacesToDisplay();
		void load();
		void unload();
		vec3 getPosition(void);
	    BlockType getBlock(int x, int y, int z);
		// void renderBoundaries() const;
};
