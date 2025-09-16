#include "Textbox.hpp"
#include <iostream>
#include <cstring>
#include <sstream>
#include <iomanip>


Textbox::Textbox() {}

void Textbox::initData(GLFWwindow* window, int x, int y, int width, int height) {
	this->window = window;
	positionX = x;
	positionY = y;
	this->width = width;
	this->height = height;
}

void Textbox::initializeFont(const std::string& fontPath, float fontSize) {
	FILE* file = fopen(fontPath.c_str(), "rb");
	if (!file) {
		std::cerr << "Failed to load font: " << fontPath << std::endl;
		return;
	}

	size_t byte_read = fread(ttfBuffer, 1, 1 << 20, file);
	fclose(file);

	if (!byte_read)
	{
		return ;
	}
	unsigned char grayscaleBitmap[512 * 512];
	stbtt_BakeFontBitmap(ttfBuffer, 0, fontSize, grayscaleBitmap, 512, 512, 32, 96, cdata);

	// Convert grayscale to RGBA
	memset(bitmap, 0, sizeof(bitmap));
	for (int i = 0; i < 512 * 512; ++i) {
		unsigned char intensity = grayscaleBitmap[i];
		bitmap[i * 4 + 0] = intensity;
		bitmap[i * 4 + 1] = intensity;
		bitmap[i * 4 + 2] = intensity;
		bitmap[i * 4 + 3] = intensity;
	}

	// Generate texture
	glGenTextures(1, &fontTexture);
	glBindTexture(GL_TEXTURE_2D, fontTexture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 512, 512, 0, GL_RGBA, GL_UNSIGNED_BYTE, bitmap);
	fontLoaded = true;
}

void Textbox::loadFont(const std::string& fontPath, float fontSize) {
	(void)fontPath;
	(void)fontSize;
	initializeFont(fontPath, fontSize);
}

void Textbox::addLine(const std::string& label, e_type type, void* value) {
	lines.push_back({label, value, type});
}

std::string to_string_with_precision(double value, int precision) {
	std::ostringstream oss;
	oss << std::fixed << std::setprecision(precision) << value;
	return oss.str();
}

std::string printMemory(size_t memory) {
	std::string sizes[4] {
		"B",
		"KB",
		"MB",
		"GB",
	};
	int i = 0;
	size_t rest;
	while (memory > 1000 && i < 3) {
		rest = memory;
		memory /= 1000;
		i++;
	}
	if (!i)
		return std::to_string(memory) + sizes[i];
	else
		return std::to_string(memory) + "." + std::to_string(rest % 1000) + sizes[i];
}

void Textbox::render() {
	if (!fontLoaded) {
		std::cerr << "Font not loaded. Cannot render text." << std::endl;
		return;
	}
	
	glUseProgram(0);
	int windowWidth, windowHeight;
	glfwGetWindowSize(window, &windowWidth, &windowHeight);

	glMatrixMode(GL_PROJECTION);
	glPushMatrix(); // Save the current projection matrix
	glLoadIdentity(); // Reset the projection matrix
	glOrtho(0.0, windowWidth, 0.0, windowHeight, -1.0, 1.0); // Set orthographic projection

	// Set up modelview matrix (make sure it's not affected by camera)
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix(); // Save the current modelview matrix
	glLoadIdentity(); // Reset the modelview matrix

	glBindTexture(GL_TEXTURE_2D, fontTexture);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	float startY = positionY - (windowHeight - 512 - 20); // Adjust for font alignment
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f); // White color for text
	for (const auto& line : lines) {
		std::string text;
		switch (line.type) {
			case DOUBLE:
				text = line.label + to_string_with_precision(*((double *)(line.value)), 1);
				break;
			case FLOAT:
			text = line.label + std::to_string(*((float *)(line.value)));
				break;
			case INT:
			text = line.label + std::to_string(*((int *)(line.value)));
				break;
			case DIRECTION:
			text = line.label + directionTab[std::clamp(*(int *)(line.value), 0, 7)].name;
				break;
			case BIOME:
			text = line.label + biomeTab[*(int *)(line.value)].name;
				break;
			case SIZE_T:
			text = line.label + printMemory(*((size_t *)(line.value)));
				break;
			case BLOCK:
			text = line.label + blockTab[*(int *)(line.value)].name;
				break;
		}
		float startX = positionX + 5;
		for (const char& ch : text) {
			if (ch >= 32) {
				stbtt_aligned_quad q;
				stbtt_GetBakedQuad(cdata, 512, 512, ch - 32, &startX, &startY, &q, 1);

				q.y0 = 512 - q.y0;
				q.y1 = 512 - q.y1;

				glBegin(GL_QUADS);
				glTexCoord2f(q.s0, q.t0); glVertex2f(q.x0, q.y0);
				glTexCoord2f(q.s1, q.t0); glVertex2f(q.x1, q.y0);
				glTexCoord2f(q.s1, q.t1); glVertex2f(q.x1, q.y1);
				glTexCoord2f(q.s0, q.t1); glVertex2f(q.x0, q.y1);
				glEnd();
			}
		}
		startY += 20; // Move to the next line
	}
	
	glBindTexture(GL_TEXTURE_2D, 0);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // Ensure transparency is applied
	glEnable(GL_DEPTH_TEST);
	glColor4f(0.0f, 0.0f, 0.0f, 0.0f); // Blue with some transparency (adjust alpha value)

	glPopMatrix(); // Restore modelview for 3D world
	glMatrixMode(GL_PROJECTION);
	glPopMatrix(); // Restore projection for 3D world
	glMatrixMode(GL_MODELVIEW);
}

Textbox::~Textbox() {
	glDeleteTextures(1, &fontTexture);
}
