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
		std::weak_ptr<Chunk> _north, _south, _east, _west; // avoid ref cycles


		bool 				_hasBufferInitialized;
		
		GLuint									_ssbo;
		std::vector<vec4>						_ssboData;
		GLuint									_vao;
		GLuint									_vbo;
		GLuint									_instanceVBO;
		std::vector<int>						_vertexData;
		std::vector<int>						_transparentVertexData;
		std::vector<DrawArraysIndirectCommand>	_transparentIndirectBufferData;
		std::vector<DrawArraysIndirectCommand>	_indirectBufferData;
		GLuint									_indirectBuffer;
		bool									_needUpdate;
		std::mutex								_sendFacesMutex;
		CaveGenerator							&_caveGen;
		std::atomic_int							_resolution;
		ThreadPool								&_pool;

	    void updateHasAllNeighbors();
	public:
		// Chunk(ivec2 pos, PerlinMap *perlinMap, CaveGenerator &caveGen, World &world, TextureManager &textureManager, int resolution = 1);
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
		void setNorthChunk(const std::shared_ptr<Chunk>& c);
		void setSouthChunk(const std::shared_ptr<Chunk>& c);
		void setEastChunk (const std::shared_ptr<Chunk>& c);
		void setWestChunk (const std::shared_ptr<Chunk>& c);
	
		std::shared_ptr<Chunk> getNorthChunk() const;
		std::shared_ptr<Chunk> getSouthChunk() const;
		std::shared_ptr<Chunk> getEastChunk () const;
		std::shared_ptr<Chunk> getWestChunk () const;
		void clearFaces();
		void loadBlocks();
		void unloadNeighbor(Direction dir);
		void unloadNeighbors();
		TopBlock getTopBlock(int localX, int localZ);
		TopBlock getTopBlockUnderPlayer(int localX, int localY, int localZ);

		std::vector<int> &getVertices();
		std::vector<int> &getTransparentVertices();
		std::vector<DrawArraysIndirectCommand> &getTransparentIndirectData();
		std::vector<DrawArraysIndirectCommand> &getIndirectData();
		std::vector<vec4> &getSSBO();
		void freeSubChunks();
		void getAABB(glm::vec3& minp, glm::vec3& maxp);
};
