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
		std::mutex				_subChunksMutex;
		bool					_isInit = false;
		World					&_world;
		TextureManager			&_textureManager;
		PerlinMap				*_perlinMap;
		Chunk					*_north = nullptr;
		Chunk					*_south = nullptr;
		Chunk					*_east = nullptr;
		Chunk					*_west = nullptr;
		std::atomic_bool		_isFullyLoaded;
		std::atomic_bool		_isBorder;
		std::atomic_bool		_facesSent;
	public:
		Chunk(vec2 position, PerlinMap *perlinMap, World &world, TextureManager &_textureManager, bool isBorder);
		~Chunk();
		void getNeighbors();
		SubChunk *getSubChunk(int y);
		void sendFacesToDisplay();
		bool isReady();
		int display();
		Chunk *getNorthChunk();
		Chunk *getSouthChunk();
		Chunk *getEastChunk();
		Chunk *getWestChunk();
		void setNorthChunk(Chunk *chunk);
		void setSouthChunk(Chunk *chunk);
		void setEastChunk(Chunk *chunk);
		void setWestChunk(Chunk *chunk);
		vec2 getPosition();
};
