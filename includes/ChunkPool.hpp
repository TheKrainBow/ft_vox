#pragma once
#include "ft_vox.hpp"
#include "Chunk.hpp"



class ChunkPool
{
	public:
		ChunkPool(World &world, TextureManager &textManager, int nbChunks);
		~ChunkPool();
		Chunk* acquire();
		void release(Chunk* chunk);
	private:
		std::vector<Chunk*> _pool;
		std::mutex _mutex;
		World &_world;
		TextureManager &_textureManager;
};