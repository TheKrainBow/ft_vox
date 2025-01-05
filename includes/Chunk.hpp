#pragma once

#include "blocks/ABlock.hpp"
#include "blocks/Dirt.hpp"
#include "blocks/Cobble.hpp"
#include "blocks/Grass.hpp"
#include "blocks/Stone.hpp"
#include "ft_vox.hpp"

#define CHUNK_SIZE_X 16
#define CHUNK_SIZE_Z 16
#define CHUNK_SIZE_Y 255

class Chunk
{
	private:
		vec2	_position;
		ABlock	*_blocks[CHUNK_SIZE_X * CHUNK_SIZE_Z * CHUNK_SIZE_Y];
	public:
		Chunk(int x, int z);
		Chunk(const Chunk& other);
		~Chunk();
		void display(void);
		void freeChunkData();
		vec2 getPosition(void);
};
