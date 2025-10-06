#pragma once

#include <GL/glew.h>
#include <vector>
#include <array>
#include <string>

class Skybox {
private:
	GLuint textureID;
	GLuint VAO, VBO;

	// Vertices for a cube centered at origin
	const float skyboxVertices[108] = {
		// positions		  
		-1.0f,  1.0f, -1.0f,
		-1.0f, -1.0f, -1.0f,
		 1.0f, -1.0f, -1.0f,
		 1.0f, -1.0f, -1.0f,
		 1.0f,  1.0f, -1.0f,
		-1.0f,  1.0f, -1.0f,

		-1.0f, -1.0f,  1.0f,
		-1.0f, -1.0f, -1.0f,
		-1.0f,  1.0f, -1.0f,
		-1.0f,  1.0f, -1.0f,
		-1.0f,  1.0f,  1.0f,
		-1.0f, -1.0f,  1.0f,

		 1.0f, -1.0f, -1.0f,
		 1.0f, -1.0f,  1.0f,
		 1.0f,  1.0f,  1.0f,
		 1.0f,  1.0f,  1.0f,
		 1.0f,  1.0f, -1.0f,
		 1.0f, -1.0f, -1.0f,

		-1.0f, -1.0f,  1.0f,
		-1.0f,  1.0f,  1.0f,
		 1.0f,  1.0f,  1.0f,
		 1.0f,  1.0f,  1.0f,
		 1.0f, -1.0f,  1.0f,
		-1.0f, -1.0f,  1.0f,

		-1.0f,  1.0f, -1.0f,
		 1.0f,  1.0f, -1.0f,
		 1.0f,  1.0f,  1.0f,
		 1.0f,  1.0f,  1.0f,
		-1.0f,  1.0f,  1.0f,
		-1.0f,  1.0f, -1.0f,

		-1.0f, -1.0f, -1.0f,
		-1.0f, -1.0f,  1.0f,
		 1.0f, -1.0f, -1.0f,
		 1.0f, -1.0f, -1.0f,
		-1.0f, -1.0f,  1.0f,
		 1.0f, -1.0f,  1.0f
	};

public:
	Skybox();
	~Skybox();
	// Explicit GL teardown (safe to call multiple times)
	void shutdownGL();
	// Build cubemap from a single equirectangular PNG (e.g., 2:1 panorama)
	// faceSize controls resolution per face; if 0, it is inferred from source size
	bool loadFromEquirectPNG(const char* filename, int faceSize = 0);
	// Build cubemap by slicing a single PNG that contains all 6 faces
	// Supported layouts (auto-detected):
	// - Horizontal cross (4x3 grid, T-layout)
	// - Vertical cross   (3x4 grid)
	// - Horizontal strip (6x1)
	// - Vertical strip   (1x6)
	// - 3x2 grid         (3x2)
	bool loadFromSinglePNG(const char* filename, bool fixSeams=false);
	// Load a cubemap from 6 PNG files in this order:
	// +X (right), -X (left), +Y (top), -Y (bottom), +Z (front), -Z (back)
	bool loadFromPNG(const std::array<std::string, 6>& faces, bool fixSeams=false);
	GLuint getTextureID(void);
void render();
};
