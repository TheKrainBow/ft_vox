#pragma once

#include "NoiseGenerator.hpp"
#include "ft_vox.hpp"
#include "TextureManager.hpp"

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
	public:
		Chunk(int x, int y, int z, NoiseGenerator::PerlinMap *perlinMap, World &world);
		~Chunk();
		void sendFacesToDisplay(void);
		void loadHeight(void);
		void loadBiome(void);
		vec3 getPosition(void);
		char getBlock(int x, int y, int z);
		void setupBuffers();
		int display(void);
		void addTextureVertex(Face face);
		void addFace(int x, int y, int z, Direction dir, TextureType texture);
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
};

bool compareUpFaces(const Chunk::Face& a, const Chunk::Face& b);
bool compareUpStep2Faces(const Chunk::Face& a, const Chunk::Face& b);
bool compareNorthFaces(const Chunk::Face& a, const Chunk::Face& b);
bool compareNorthStep2Faces(const Chunk::Face& a, const Chunk::Face& b);
bool compareEastFaces(const Chunk::Face& a, const Chunk::Face& b);
bool compareEastStep2Faces(const Chunk::Face& a, const Chunk::Face& b);