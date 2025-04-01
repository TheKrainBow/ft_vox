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
			glm::vec3	position;
			glm::vec2	size;
			TextureType	texture;
			Direction	direction;
		} Face;
	private:
		vec3				_position;
		std::vector<char>	_blocks;
		double				**_heightMap;
		World				&_world;
		Chunk				&_chunk;

    	std::vector<Face>	_faces[6];
		bool				_loaded = false;
		bool				_hasSentFaces = false;
		bool				_isFullyLoaded = true;
		bool				_hasBufferInitialized = false;
		GLuint				_vao;
		GLuint				_vbo;
		GLuint				_instanceVBO;
		std::vector<int>	_vertexData;
		TextureManager		&_textManager;
		bool				_needUpdate;
		Chrono chrono;
	public:
		SubChunk(vec3 position, PerlinMap *perlinMap, Chunk &chunk, World &world, TextureManager &textManager);
		~SubChunk();
		void setupBuffers();
		int display(void);
		void addTextureVertex(Face face);
		void addFace(vec3 position, Direction dir, TextureType texture);
		void loadHeight();
		void loadBiome();
		void loadOcean(int x, int z, size_t ground);
		void loadPlaine(int x, int z, size_t ground);
		void loadMountain(int x, int z, size_t ground);
		vec3 getPosition(void);
		char getBlock(vec3 position);
		void setBlock(vec3 position, char block);
		void sendFacesToDisplay();
		void pushVerticesToOpenGL();
		vec2 getBorderWarping(double x, double z,  NoiseGenerator &noise_gen) const;
		double getContinentalNoise(vec2 pos, NoiseGenerator &noise_gen);
		double getMinHeight(vec2 pos, NoiseGenerator &noise_gen);
		void clearFaces();
	private:
		void addBlock(BlockType block, vec3 position, TextureType down, TextureType up, TextureType north, TextureType south, TextureType east, TextureType west);
		void addUpFace(BlockType block, vec3 position, TextureType texture);
		void addDownFace(BlockType block, vec3 position, TextureType texture);
		void addNorthFace(BlockType block, vec3 position, TextureType texture);
		void addSouthFace(BlockType block, vec3 position, TextureType texture);
		void addEastFace(BlockType block, vec3 position, TextureType texture);
		void addWestFace(BlockType block, vec3 position, TextureType texture);
	
		void processFaces();
		void processUpVertex();
		void processDownVertex();
		void processNorthVertex();
		void processSouthVertex();
		void processEastVertex();
		void processWestVertex();
		void initGLBuffer();
};

bool compareUpFaces(const SubChunk::Face& a, const SubChunk::Face& b);
bool compareUpStep2Faces(const SubChunk::Face& a, const SubChunk::Face& b);
bool compareNorthFaces(const SubChunk::Face& a, const SubChunk::Face& b);
bool compareNorthStep2Faces(const SubChunk::Face& a, const SubChunk::Face& b);
bool compareEastFaces(const SubChunk::Face& a, const SubChunk::Face& b);
bool compareEastStep2Faces(const SubChunk::Face& a, const SubChunk::Face& b);