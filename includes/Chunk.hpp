#pragma once

#include "NoiseGenerator.hpp"
#include "ft_vox.hpp"
#include "TextureManager.hpp"
#include "define.hpp"
#include "globals.hpp"
#include "World.hpp"

class BiomeGenerator;
class World;

class Chunk
{
	private:
		vec3	_position;
		std::vector<char>	_blocks;
		double	_perlinMap[CHUNK_SIZE * CHUNK_SIZE];
		World	&_world;
		TextureManager &_textManager;
		bool	loaded = false;
	public:
		Chunk(int x, int y, int z, NoiseGenerator::PerlinMap *perlinMap, World &world, TextureManager &textManager);
		~Chunk();
		vec2 getBorderWarping(double x, double z,  NoiseGenerator &noise_gen) const;
		double getContinentalNoise(vec2 pos, NoiseGenerator &noise_gen);
		double getMinHeight(vec2 pos, NoiseGenerator &noise_gen);
		void loadHeight();
		void loadBiome();
		vec3 getPosition(void);
	    char getBlock(int x, int y, int z);
		void sendFacesToDisplay();
	private:
		void addBlock(int blockX, int blockY, int blockZ, TextureType down, TextureType up, TextureType north, TextureType south, TextureType east, TextureType west);
};
