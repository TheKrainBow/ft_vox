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
		std::atomic_bool					_isFullyLoaded;
		std::atomic_bool					_facesSent;
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
		std::mutex							_sendFaceMutex;
		// Render buffer data

		bool 				_hasBufferInitialized;
		
		GLuint									_ssbo;
		std::vector<glm::vec4>					_ssboData;
		GLuint									_vao;
		GLuint									_vbo;
		GLuint									_instanceVBO;
		std::vector<int>						_vertexData;
		std::vector<DrawArraysIndirectCommand>	_indirectBufferData;
		GLuint									_indirectBuffer;
		bool									_needUpdate;
	
	public:
		Chunk(ivec2 pos, PerlinMap *perlinMap, World &world, TextureManager &textureManager);
		~Chunk();
		void getNeighbors();
		SubChunk *getSubChunk(int y);
		void sendFacesToDisplay();
		bool isReady();
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
		void initGLBuffer();
		void pushVerticesToOpenGL(bool isTransparent);
		void clearFaces();

		std::vector<int> getVertices();
		std::vector<DrawArraysIndirectCommand> getIndirectData();
		std::vector<vec4> getSSBO();
};
