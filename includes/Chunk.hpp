#pragma once

#include "NoiseGenerator.hpp"
#include "ft_vox.hpp"
#include "TextureManager.hpp"
#include "define.hpp"
#include "World.hpp"

class BiomeGenerator;
class World;

class Chunk
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
		GLuint				_vao;
		GLuint				_vbo;
		GLuint				_instanceVBO;
		std::vector<int>	_vertexData;
		TextureManager &_textManager;
	public:
		Chunk(int x, int y, int z, NoiseGenerator::PerlinMap *perlinMap, World &world, TextureManager &textManager);
		~Chunk();
		void setupBuffers();
		int display(void);
		void addTextureVertex(Face face);
		void addFace(int x, int y, int z, Direction dir, TextureType texture);
		void loadHeight();
		void loadBiome();
		vec3 getPosition(void);
	    char getBlock(int x, int y, int z);
		void sendFacesToDisplay();
		vec2 getBorderWarping(double x, double z,  NoiseGenerator &noise_gen) const;
		double getContinentalNoise(vec2 pos, NoiseGenerator &noise_gen);
		double getMinHeight(vec2 pos, NoiseGenerator &noise_gen);
		// void renderBoundaries() const;
	private:
		void addBlock(int x, int y, int z, TextureType down, TextureType up, TextureType north, TextureType south, TextureType east, TextureType west);
		void processFaces();
		void processUpVertex();
		void processDownVertex();
		void processNorthVertex();
		void processSouthVertex();
		void processEastVertex();
		void processWestVertex();
		void clearFaces();
};

bool compareUpFaces(const Chunk::Face& a, const Chunk::Face& b);
bool compareUpStep2Faces(const Chunk::Face& a, const Chunk::Face& b);
bool compareNorthFaces(const Chunk::Face& a, const Chunk::Face& b);
bool compareNorthStep2Faces(const Chunk::Face& a, const Chunk::Face& b);
bool compareEastFaces(const Chunk::Face& a, const Chunk::Face& b);
bool compareEastStep2Faces(const Chunk::Face& a, const Chunk::Face& b);