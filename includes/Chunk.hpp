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
		Chunk								*_north = nullptr;
		Chunk								*_south = nullptr;
		Chunk								*_east = nullptr;
		Chunk								*_west = nullptr;


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
	public:
		Chunk(ivec2 pos, PerlinMap *perlinMap, World &world, TextureManager &textureManager, int resolution = 1);
		~Chunk();
		void getNeighbors();
		std::atomic_int						_resolution;
		SubChunk *getSubChunk(int y);
		void updateResolution(int newResolution);
		void sendFacesToDisplay();
		bool isReady();
		ivec2 getPosition();
		size_t getMemorySize();
		void setWestChunk(Chunk *chunk);
		void setNorthChunk(Chunk *chunk);
		void setEastChunk(Chunk *chunk);
		void setSouthChunk(Chunk *chunk);
		Chunk *getNorthChunk();
		Chunk *getSouthChunk();
		Chunk *getWestChunk();
		Chunk *getEastChunk();
		void clearFaces();
		void loadBlocks();
		void unloadNeighbor(Direction dir);
		void unloadNeighbors();

		std::vector<int> &getVertices();
		std::vector<int> &getTransparentVertices();
		std::vector<DrawArraysIndirectCommand> &getTransparentIndirectData();
		std::vector<DrawArraysIndirectCommand> &getIndirectData();
		std::vector<vec4> &getSSBO();
		void freeSubChunks();
};
