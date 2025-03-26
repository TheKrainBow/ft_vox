#pragma once

#include "ft_vox.hpp"
#include "NoiseGenerator.hpp"
#include "World.hpp"
#include "TextureManager.hpp"
#include "SubChunk.hpp"
#include "Chrono.hpp"
#include <future>

class SubChunk;
class World;

class Chunk
{
	private:
		vec2					_position;
		std::atomic_bool		_isFullyLoaded;
		std::atomic_bool		_facesSent;
		std::unordered_map<int, SubChunk *>	_subChunks;
		std::mutex							_subChunksMutex;
		std::atomic_bool					_isInit;
		World								&_world;
		TextureManager						&_textureManager;
		PerlinMap							*_perlinMap;
		std::atomic_bool					_facesSent;
		Chrono								_chrono;
		Chunk					*_north = nullptr;
		Chunk					*_south = nullptr;
		Chunk					*_east = nullptr;
		Chunk					*_west = nullptr;
	public:
		Chunk(vec2 pos, PerlinMap *perlinMap, World &world, TextureManager &textureManager);
		~Chunk();
		void getNeighbors();
		SubChunk *getSubChunk(int y);
		void sendFacesToDisplay();
		bool isReady();
		int display();
		vec2 getPosition();
		void setWestChunk(Chunk *chunk);
		void setNorthChunk(Chunk *chunk);
		void setEastChunk(Chunk *chunk);
		void setSouthChunk(Chunk *chunk);
};
