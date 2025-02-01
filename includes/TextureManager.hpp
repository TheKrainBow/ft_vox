#pragma once

#include "ft_vox.hpp"
#include "Texture.hpp"
#include "Camera.hpp"

struct t_rgba {
	uint8_t r, g, b, a;
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
# define TEXTURE_SIZE 16

class TextureManager {
	private:
		Texture *_textures[N_TEXTURES];
		GLuint _mergedTextureID = 0;
		GLuint _textureArrayID = 0;
	public:
		TextureManager(); // Constructor declaration
		~TextureManager(); // Destructor declaration
		void loadTexturesArray(std::vector<std::pair<TextureType, std::string>> data);
		GLuint getTextureArray() const;
		GLuint getTexture(TextureType text);
		void addTextureVertex(TextureType type, Direction dir, int x, int y, int );
		void resetTextureVertex();
		int displayAllTexture(Camera &cam);
		void processTextureVertex();
};
