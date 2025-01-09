#pragma once

#include "ft_vox.hpp"
#include "Texture.hpp"

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
	T_SAND
};

class TextureManager {
	private:
		std::map<TextureType, Texture *> _textures;
	public:
		TextureManager(); // Constructor declaration
		~TextureManager(); // Destructor declaration

		void loadTexture(TextureType type, std::string path);
		void addTextureVertex(TextureType type, int x, int y, int z);
		void resetTextureVertex(TextureType type);
		void resetAllTextureVertex();
		void displayTexture(TextureType type);
		void displayAllTexture();
	private:
		// Declaring external functions that are not part of the class
		t_rgb *loadPPM(const std::string &path, int &width, int &height);
};
