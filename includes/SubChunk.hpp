#pragma once

#include "NoiseGenerator.hpp"
#include "ft_vox.hpp"
#include "TextureManager.hpp"
#include "define.hpp"
#include "World.hpp"
#include "Chrono.hpp"

class World;
class Chunk;
class CaveGenerator;

class SubChunk
{
	public:
		typedef struct s_Face
		{
			ivec3	position;
			ivec2	size;
			TextureType	texture;
			Direction	direction;
		} Face;
	private:
		ivec3						_position;
		int							_resolution;
		size_t						_memorySize = 0;
		int							_chunkSize;
		std::unique_ptr<uint8_t[]>	_blocks;
		double						**_heightMap;
		Biome						**_biomeMap;
		double						**_treeMap;
		World						&_world;
		Chunk						&_chunk;

		bool						_loaded = false;
		bool						_hasSentFaces = false;
		bool						_isFullyLoaded = false;
		bool						_hasBufferInitialized = false;

		std::vector<Face>			_faces[6];

		std::mutex					_dataMutex;
		
		std::vector<int>			_vertexData;

		std::vector<Face>			_transparentFaces[6];
		std::vector<int>			_transparentVertexData;

		TextureManager				&_textManager;
		bool						_needUpdate;
		bool						_needTransparentUpdate;

		CaveGenerator				&_caveGen;
	public:
		SubChunk(ivec3 position, PerlinMap *perlinMap, CaveGenerator &caveGen, Chunk &chunk, World &world, TextureManager &textManager, int resolution = 1);
		~SubChunk();
		void markLoaded(bool loaded = true);
		void addTextureVertex(Face face, std::vector<int> *_vertexData);
		void addFace(ivec3 position, Direction dir, TextureType texture, bool isTransparent);
		void loadHeight(int prevResolution);
		void loadBiome(int prevResolution);
		void loadOcean(int x, int z, size_t ground, size_t adjustedOceanHeight);
		void loadPlaine(int x, int z, size_t ground);
		void loadMountain(int x, int z, size_t ground);
		void loadDesert(int x, int z, size_t ground);
		void loadBeach(int x, int z, size_t ground);
		void loadSnowy(int x, int z, size_t ground);
		void loadForest(int x, int z, size_t ground);
		void plantTree(int x, int y, int z, double proba);
		void loadTree(int x, int z);
		ivec3 getPosition(void);
		char getBlock(ivec3 position);
		bool isNeighborTransparent(ivec3 position, Direction dir, char viewerBlock, int viewerResolution);
		void setBlock(int x, int y, int z, char block);
		void sendFacesToDisplay();
		ivec2 getBorderWarping(double x, double z,  NoiseGenerator &noise_gen) const;
		size_t getMemorySize();
		void clearFaces();
		std::vector<int> &getVertices();
		std::vector<int> &getTransparentVertices();
		void updateResolution(int resolution, PerlinMap *perlinMap);
	private:
		void addBlock(BlockType block, ivec3 position, TextureType down, TextureType up, TextureType north, TextureType south, TextureType east, TextureType west, bool transparent);
		void addUpFace(BlockType block, ivec3 position, TextureType texture, bool isTransparent);
		void addDownFace(BlockType block, ivec3 position, TextureType texture, bool isTransparent);
		void addNorthFace(BlockType block, ivec3 position, TextureType texture, bool isTransparent);
		void addSouthFace(BlockType block, ivec3 position, TextureType texture, bool isTransparent);
		void addEastFace(BlockType block, ivec3 position, TextureType texture, bool isTransparent);
		void addWestFace(BlockType block, ivec3 position, TextureType texture, bool isTransparent);
	
		void processFaces(bool isTransparent);
		void processUpVertex(std::vector<Face> *faces, std::vector<int> *vertexData);
		void processDownVertex(std::vector<Face> *faces, std::vector<int> *vertexData);
		void processNorthVertex(std::vector<Face> *faces, std::vector<int> *vertexData);
		void processSouthVertex(std::vector<Face> *faces, std::vector<int> *vertexData);
		void processEastVertex(std::vector<Face> *faces, std::vector<int> *vertexData);
		void processWestVertex(std::vector<Face> *faces, std::vector<int> *vertexData);
};

bool compareUpFaces(const SubChunk::Face& a, const SubChunk::Face& b);
bool compareUpStep2Faces(const SubChunk::Face& a, const SubChunk::Face& b);
bool compareNorthFaces(const SubChunk::Face& a, const SubChunk::Face& b);
bool compareNorthStep2Faces(const SubChunk::Face& a, const SubChunk::Face& b);
bool compareEastFaces(const SubChunk::Face& a, const SubChunk::Face& b);
bool compareEastStep2Faces(const SubChunk::Face& a, const SubChunk::Face& b);
