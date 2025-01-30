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
	T_SAND
};

# define N_TEXTURES 5

class TextureManager {
	private:
		Texture *_textures[N_TEXTURES];
		GLuint mergedTextureId = 0;
	public:
		TextureManager(); // Constructor declaration
		~TextureManager(); // Destructor declaration

		void loadTexture(TextureType type, std::string path);
		void loadTextures(std::vector<std::pair<TextureType, std::string>> data);
		GLuint getMergedText() const;
		GLuint getTexture(TextureType text);
		void addTextureVertex(TextureType type, Direction dir, int x, int y, int );
		void resetTextureVertex();
		int displayAllTexture(Camera &cam);
		void processTextureVertex();
	private:
		// Declaring external functions that are not part of the class
		t_rgb *loadPPM(const std::string &path, int &width, int &height);
};
