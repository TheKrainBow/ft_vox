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
		ivec2								_position;
		int									_resolution;
		std::atomic_bool					_isFullyLoaded;
		std::atomic_bool					_facesSent;
		bool								_hasAllNeighbors;
		std::unordered_map<int, SubChunk *>	_subChunks;
		std::mutex							_subChunksMutex;
		std::atomic_bool					_isInit;
		World								&_world;
		TextureManager						&_textureManager;
		PerlinMap							*_perlinMap;
		Chrono								_chrono;
		Chunk								*_north = nullptr;
		Chunk								*_south = nullptr;
		Chunk								*_east = nullptr;
		Chunk								*_west = nullptr;
		int									_loads = 0;
		std::mutex							_sendFaceMutex;
	public:
		Chunk(ivec2 pos, PerlinMap *perlinMap, World &world, TextureManager &textureManager, int resolution = 1);
		~Chunk();
		void getNeighbors();
		SubChunk *getSubChunk(int y);
		void sendFacesToDisplay();
		bool isReady();
		int display();
		int displayTransparent();
		ivec2 getPosition();
		void setWestChunk(Chunk *chunk);
		void setNorthChunk(Chunk *chunk);
		void setEastChunk(Chunk *chunk);
		void setSouthChunk(Chunk *chunk);
		Chunk *getNorthChunk();
		Chunk *getSouthChunk();
		Chunk *getWestChunk();
		Chunk *getEastChunk();
};
