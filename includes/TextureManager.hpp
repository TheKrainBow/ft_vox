#pragma once

#include "ft_vox.hpp"
#include "Texture.hpp"
#include "Camera.hpp"

#define NB_TEXTURES 2

struct t_rgb {
	uint8_t r, g, b;
};

enum TextureType {
	T_DIRT,
	T_COBBLE,
	T_STONE,
	T_GRASS_SIDE,
	T_GRASS_TOP,
};

class TextureManager {
	private:
		std::map<TextureType, Texture *> _textures;
	public:
		TextureManager(); // Constructor declaration
		~TextureManager(); // Destructor declaration

		void loadTexture(TextureType type, std::string path);
		void addTextureVertex(TextureType type, e_direction dir, int x, int y, int z, double u, double v);
		void resetTextureVertex();
		int displayAllTexture(Camera &cam);
	private:
		// Declaring external functions that are not part of the class
		t_rgb *loadPPM(const std::string &path, int &width, int &height);
};
