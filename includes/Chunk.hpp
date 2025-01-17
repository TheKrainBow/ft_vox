#pragma once

#include "NoiseGenerator.hpp"
#include "ft_vox.hpp"
#include "TextureManager.hpp"

class World;

class Chunk
{
	private:
		vec3	_position;
		std::vector<char>	_blocks;
		double	_perlinMap[CHUNK_SIZE * CHUNK_SIZE];
		World	&_world;
		bool	loaded = false;
	public:
		Chunk(int x, int y, int z, NoiseGenerator::PerlinMap *perlinMap, World &world);
		~Chunk();
		void sendFacesToDisplay();
		void load();
		void positiveLoad();
		void negativeLoad();
		vec3 getPosition(void);
	    char getBlock(int x, int y, int z);
		// void renderBoundaries() const;
	private:
		void addBlock(int x, int y, int z, TextureType down, TextureType up, TextureType north, TextureType south, TextureType east, TextureType west);
};
