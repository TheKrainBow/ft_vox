#pragma once

#include "NoiseGenerator.hpp"
#include "ft_vox.hpp"
#include "TextureManager.hpp"

#define CHUNK_SIZE 16

class World;

class Chunk
{
	private:
		vec3	_position;
		char	_blocks[CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE];
		double	_perlinMap[CHUNK_SIZE * CHUNK_SIZE];
		World	&_world;
	public:
		Chunk(int x, int y, int z, World &world);
		~Chunk();
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
