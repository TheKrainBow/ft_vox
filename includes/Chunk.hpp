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
		bool	_loaded = false;
		int		_resolution;
	public:
		Chunk(int x, int y, int z, NoiseGenerator::PerlinMap *perlinMap, World &world, int resolution = 1);
		~Chunk();
		void sendFacesToDisplay();
		void loadHeight();
		void loadBiome();
		vec3 getPosition(void);
	    char getBlock(int x, int y, int z);
		int getResolution();
		void updateResolution(int resolution);
		// void renderBoundaries() const;
	private:
		void addBlock(int x, int y, int z, TextureType down, TextureType up, TextureType north, TextureType south, TextureType east, TextureType west);
};
