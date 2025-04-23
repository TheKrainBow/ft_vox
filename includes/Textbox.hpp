#ifndef TEXTBOX_HPP
#define TEXTBOX_HPP

#include <GL/glew.h> // For GLEW
#include <GL/gl.h>   // OpenGL headers
#include <GLFW/glfw3.h>
#include <string>
#include <vector>
#include "Camera.hpp"
#include "stb_truetype.hpp"

struct direction_pair
{
	e_direction direction;
	std::string name;
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
		DIRECTION
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

	void initializeFont(const std::string& fontPath, float fontSize);

public:
	Textbox();
	void loadFont(const std::string& fontPath, float fontSize);
	void addLine(const std::string& label, e_type type, void* value);
	void render();
	void initData(GLFWwindow* window, int x, int y, int width, int height);
	~Textbox();
};

#endif
