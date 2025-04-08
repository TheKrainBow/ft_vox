#pragma once

#include "NoiseGenerator.hpp"
#include "ft_vox.hpp"
#include "TextureManager.hpp"
#include "define.hpp"
#include "World.hpp"
#include "Chrono.hpp"

class BiomeGenerator;
class World;
class Chunk;

class SubChunk
{
	public:
		typedef struct s_Face
		{
			glm::ivec3	position;
			glm::ivec2	size;
			TextureType	texture;
			Direction	direction;
		} Face;
	private:
		vec3				_position;
		std::vector<char>	_blocks;
		double				**_heightMap;
		World				&_world;
		Chunk				&_chunk;

		bool				_loaded = false;
		bool				_hasSentFaces = false;
		bool				_isFullyLoaded = true;
		bool				_hasBufferInitialized = false;

		std::vector<Face>	_faces[6];
		std::vector<int>	_vertexData;

		std::vector<Face>	_transparentFaces[6];
		std::vector<int>	_transparentVertexData;
	
		TextureManager		&_textManager;
		bool				_needUpdate;
		bool				_needTransparentUpdate;
		Chrono chrono;
	public:
		SubChunk(vec3 position, PerlinMap *perlinMap, Chunk &chunk, World &world, TextureManager &textManager);
		~SubChunk();
		void setupBuffers();
		void addTextureVertex(Face face, std::vector<int> *_vertexData);
		void addFace(vec3 position, Direction dir, TextureType texture, bool isTransparent);
		void loadHeight();
		void loadBiome();
		void loadOcean(int x, int z, size_t ground);
		void loadPlaine(int x, int z, size_t ground);
		void loadMountain(int x, int z, size_t ground);
		vec3 getPosition(void);
		char getBlock(vec3 position);
		void setBlock(vec3 position, char block);
		void sendFacesToDisplay();
		void pushVerticesToOpenGL(bool isTransparent);
		vec2 getBorderWarping(double x, double z,  NoiseGenerator &noise_gen) const;
		double getContinentalNoise(vec2 pos, NoiseGenerator &noise_gen);
		double getMinHeight(vec2 pos, NoiseGenerator &noise_gen);
		void clearFaces();
		std::vector<int> getVertices();
	private:
		void addBlock(BlockType block, vec3 position, TextureType down, TextureType up, TextureType north, TextureType south, TextureType east, TextureType west, bool transparent);
		void addUpFace(BlockType block, vec3 position, TextureType texture, bool isTransparent);
		void addDownFace(BlockType block, vec3 position, TextureType texture, bool isTransparent);
		void addNorthFace(BlockType block, vec3 position, TextureType texture, bool isTransparent);
		void addSouthFace(BlockType block, vec3 position, TextureType texture, bool isTransparent);
		void addEastFace(BlockType block, vec3 position, TextureType texture, bool isTransparent);
		void addWestFace(BlockType block, vec3 position, TextureType texture, bool isTransparent);
	
		void processFaces(bool isTransparent);
		void processUpVertex(std::vector<Face> *faces, std::vector<int> *vertexData);
		void processDownVertex(std::vector<Face> *faces, std::vector<int> *vertexData);
		void processNorthVertex(std::vector<Face> *faces, std::vector<int> *vertexData);
		void processSouthVertex(std::vector<Face> *faces, std::vector<int> *vertexData);
		void processEastVertex(std::vector<Face> *faces, std::vector<int> *vertexData);
		void processWestVertex(std::vector<Face> *faces, std::vector<int> *vertexData);
		void initGLBuffer();
};

bool compareUpFaces(const SubChunk::Face& a, const SubChunk::Face& b);
bool compareUpStep2Faces(const SubChunk::Face& a, const SubChunk::Face& b);
bool compareNorthFaces(const SubChunk::Face& a, const SubChunk::Face& b);
bool compareNorthStep2Faces(const SubChunk::Face& a, const SubChunk::Face& b);
bool compareEastFaces(const SubChunk::Face& a, const SubChunk::Face& b);
bool compareEastStep2Faces(const SubChunk::Face& a, const SubChunk::Face& b);