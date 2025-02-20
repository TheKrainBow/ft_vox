#pragma once

#include "NoiseGenerator.hpp"
#include "ft_vox.hpp"
#include "TextureManager.hpp"
#include "define.hpp"
#include "World.hpp"

class BiomeGenerator;
class World;

class SubChunk
{
	public:
		typedef struct s_Face {
			glm::vec3	position;
			glm::vec2	size;
			TextureType	texture;
			Direction	direction;
		} Face;
	private:
		vec3				_position;
		std::vector<char>	_blocks;
		double				_perlinMap[CHUNK_SIZE * CHUNK_SIZE];
		World				&_world;

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
	public:
		SubChunk(vec3 position, PerlinMap *perlinMap, World &world, TextureManager &textManager);
		~SubChunk();
		void setupBuffers();
		int display(void);
		void addTextureVertex(Face face);
		void addFace(vec3 position, Direction dir, TextureType texture);
		void loadHeight();
		void loadBiome();
		vec3 getPosition(void);
	    char getBlock(vec3 position);
		void sendFacesToDisplay();
		vec2 getBorderWarping(double x, double z,  NoiseGenerator &noise_gen) const;
		double getContinentalNoise(vec2 pos, NoiseGenerator &noise_gen);
		double getMinHeight(vec2 pos, NoiseGenerator &noise_gen);
		// void renderBoundaries() const;
	private:
		void addBlock(vec3 position, TextureType down, TextureType up, TextureType north, TextureType south, TextureType east, TextureType west);
		void addUpFace(vec3 position, TextureType texture);
		void addDownFace(vec3 position, TextureType texture);
		void addNorthFace(vec3 position, TextureType texture);
		void addSouthFace(vec3 position, TextureType texture);
		void addEastFace(vec3 position, TextureType texture);
		void addWestFace(vec3 position, TextureType texture);
	
		void processFaces();
		void processUpVertex();
		void processDownVertex();
		void processNorthVertex();
		void processSouthVertex();
		void processEastVertex();
		void processWestVertex();
		void clearFaces();
		void initGLBuffer();
};

bool compareUpFaces(const SubChunk::Face& a, const SubChunk::Face& b);
bool compareUpStep2Faces(const SubChunk::Face& a, const SubChunk::Face& b);
bool compareNorthFaces(const SubChunk::Face& a, const SubChunk::Face& b);
bool compareNorthStep2Faces(const SubChunk::Face& a, const SubChunk::Face& b);
bool compareEastFaces(const SubChunk::Face& a, const SubChunk::Face& b);
bool compareEastStep2Faces(const SubChunk::Face& a, const SubChunk::Face& b);