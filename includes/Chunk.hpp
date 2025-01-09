#pragma once

#include "blocks/ABlock.hpp"
#include "blocks/Dirt.hpp"
#include "blocks/Cobble.hpp"
#include "blocks/Grass.hpp"
#include "blocks/Stone.hpp"
#include "NoiseGenerator.hpp"
#include "ft_vox.hpp"

#define CHUNK_SIZE_X 16
#define CHUNK_SIZE_Z 16
#define CHUNK_SIZE_Y 255
#define CHUNK_SIZE CHUNK_SIZE_X * CHUNK_SIZE_Z * CHUNK_SIZE_Y

class Chunk
{
	private:
		vec2	_position;
		ABlock	*_blocks[CHUNK_SIZE];
	public:
		Chunk(int chunkX, int z, NoiseGenerator &noise_gen);
		~Chunk();
		void display(Camera &camera);
		void freeChunkData();
		vec2 getPosition(void);
		void renderBoundaries() const;
};
