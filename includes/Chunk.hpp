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
		GLuint				_vao;
		GLuint				_vbo;
		GLuint				_instanceVBO;
		std::vector<int>	_vertexData;
	public:
		Chunk(int x, int y, int z, NoiseGenerator::PerlinMap *perlinMap, World &world);
		~Chunk();
		void sendFacesToDisplay(void);
		void loadHeight(void);
		void loadBiome(void);
		vec3 getPosition(void);
		char getBlock(int x, int y, int z);
		void setupBuffers();
		int display(void);
		void addTextureVertex(int x, int y, int z, int direction, int textureID);
		// void renderBoundaries() const;
	private:
		void addBlock(int x, int y, int z, TextureType down, TextureType up, TextureType north, TextureType south, TextureType east, TextureType west);
};
