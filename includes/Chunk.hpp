#pragma once

#include "ft_vox.hpp"
#include "NoiseGenerator.hpp"
#include "World.hpp"
#include "TextureManager.hpp"
#include "SubChunk.hpp"

class SubChunk;
class World;

class Chunk
{
	private:
		vec2					_position;
		std::vector<SubChunk *>	_subChunks;
		World					&_world;
		TextureManager			&_textureManager;
		PerlinMap				*_perlinMap;
	public:
		Chunk(vec2 position, PerlinMap *perlinMap, World &world, TextureManager &_textureManager);
		~Chunk();
		SubChunk *getSubChunk(int y);
		void display();
};
