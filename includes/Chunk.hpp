#pragma once

#include "ft_vox.hpp"
#include "NoiseGenerator.hpp"
#include "World.hpp"
#include "TextureManager.hpp"
#include "SubChunk.hpp"
#include "Chrono.hpp"

#include "ThreadPool.hpp"
#include <future>

class SubChunk;
class World;
class CaveGenerator;

class Chunk : public std::enable_shared_from_this<Chunk>
{
	private:
		ivec2								_position;
		size_t								_memorySize = 0;
		std::atomic_bool					_isFullyLoaded;
		std::atomic_bool					_facesSent;
		std::atomic_bool					_hasAllNeighbors;
		std::unordered_map<int, SubChunk *>	_subChunks;
		std::mutex							_subChunksMutex;
		std::atomic_bool					_isInit;
		World								&_world;
		TextureManager						&_textureManager;
		PerlinMap							*_perlinMap;
		Chrono								_chrono;
		Chunk *_north;
		Chunk *_south;
		Chunk *_east;
		Chunk *_west;


		std::vector<vec4>						_ssboData;
		std::vector<int>						_vertexData;
		std::vector<int>						_transparentVertexData;
		std::vector<DrawArraysIndirectCommand>	_transparentIndirectBufferData;
		std::vector<DrawArraysIndirectCommand>	_indirectBufferData;
		std::mutex								_sendFacesMutex;
		CaveGenerator							&_caveGen;
		std::atomic_int							_resolution;
		ThreadPool								&_pool;
		std::atomic_bool						_isBuilding;
		std::atomic_bool						_isModified;
	public:
		Chunk(ivec2 pos, PerlinMap *perlinMap, CaveGenerator &caveGen, World &world, TextureManager &textureManager, ThreadPool &pool, int resolution = 1);
		~Chunk();
		void getNeighbors();
		SubChunk *getSubChunk(int y);
		SubChunk *getOrCreateSubChunk(int y, bool generate = true);
		void updateResolution(int newResolution);
		void sendFacesToDisplay();
		bool isReady();
		std::atomic_int	&getResolution();
		ivec2 getPosition();
		size_t getMemorySize();
		void setNorthChunk(Chunk *c);
		void setSouthChunk(Chunk *c);
		void setEastChunk (Chunk *c);
		void setWestChunk (Chunk *c);
	
		Chunk *getNorthChunk();
		Chunk *getSouthChunk();
		Chunk *getEastChunk ();
		Chunk *getWestChunk ();
	
		void clearFaces();
		void loadBlocks();
		void unloadNeighbor(Direction dir);
		void unloadNeighbors();
		TopBlock getTopBlock(int localX, int localZ);

		std::vector<int> &getVertices();
		std::vector<int> &getTransparentVertices();
		std::vector<DrawArraysIndirectCommand> &getTransparentIndirectData();
		std::vector<DrawArraysIndirectCommand> &getIndirectData();
		std::vector<vec4> &getSSBO();
		void freeSubChunks();
		void getAABB(glm::vec3& minp, glm::vec3& maxp);
		void snapshotDisplayData(
			std::vector<int>&							outSolidVerts,
			std::vector<DrawArraysIndirectCommand>&		outSolidCmds,
			std::vector<vec4>&							outSSBO,
			std::vector<int>&							outTranspVerts,
			std::vector<DrawArraysIndirectCommand>&		outTranspCmds);
		TopBlock getFirstSolidBelow(int localX, int startLocalY, int localZ, int startSubY);
		bool isBuilding() const;
		void setAsModified();
		bool getModified() const;
	private:	
		void updateHasAllNeighbors();
};
