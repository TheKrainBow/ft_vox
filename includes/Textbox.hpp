#ifndef TEXTBOX_HPP
#define TEXTBOX_HPP

#include <GL/glew.h>
#include <GL/gl.h>
#include <GLFW/glfw3.h>
#include <string>
#include <vector>
#include <list>
#include "Camera.hpp"
#include "stb_truetype.hpp"
#include "stb_image.h"
#include "NoiseGenerator.hpp"

struct direction_pair
{
	e_direction direction;
	std::string name;
};

struct biome_pair
{
	Biome biome;
	std::string name;
};

struct block_pair
{
	BlockType block;
	std::string name;
};

const block_pair blockTab[NB_BLOCKS] = {
	{air, "None"},
	{stone, "Stone"},
	{cobble, "Cobble"},
	{bedrock, "Bedrock"},
	{dirt, "Dirt"},
	{grass, "Grass"},
	{sand, "Sand"},
	{water, "Water"},
	{snow, "Snow"},
	{oak_log, "Oak log"},
	{leaf, "Leaf"}
};

const biome_pair biomeTab[NB_BIOMES] = {
	{PLAINS, "Plains"},
	{DESERT, "Desert"},
	{SNOWY, "Snow"},
	{MOUNTAINS, "Mountains"},
	{FOREST, "Forest"},
	{OCEAN, "Ocean"},
	{BEACH, "Beach"}
};

const direction_pair directionTab[8] = {
	{N, "North"},
	{NW, "North West"},
	{W, "West"},
	{SW, "South West"},
	{S, "South"},
	{SE, "South East"},
	{E, "East"},
	{NE, "North East"}
};

class Textbox {
public:
		enum e_type {
			DOUBLE,
			INT,
			FLOAT,
			DIRECTION,
			BIOME,
			SIZE_T,
			BLOCK,
			STRING
		};
private:
	struct Line {
		std::string label;
		void* value;
		e_type type;
	};

	std::vector<Line> lines;
	GLFWwindow* window;
	int positionX, positionY, width, height;
	stbtt_bakedchar cdata[96];
	unsigned char ttfBuffer[1 << 20];
	unsigned char bitmap[512 * 512 * 4];
	GLuint fontTexture;
	bool fontLoaded;
	// Use list to keep pointers to stored strings stable
	std::list<std::string> _ownedStrings;

	void initializeFont(const std::string& fontPath, float fontSize);

public:
	Textbox();
	void loadFont(const std::string& fontPath, float fontSize);
	void addLine(const std::string& label, e_type type, void* value);
	void addStaticText(const std::string& text);
	void render();
	void initData(GLFWwindow* window, int x, int y, int width, int height);
	~Textbox();
};

#endif
