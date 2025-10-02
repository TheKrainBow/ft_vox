#pragma once

#include "ft_vox.hpp"
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
	T_SAND,
	T_WATER,
	T_SNOW,
	T_BEDROCK,
	T_LOG_SIDE,
	T_LOG_TOP,
	T_LEAF,
	T_CACTUS_SIDE,
	T_CACTUS_TOP,
};

# define N_TEXTURES 14
# define TEXTURE_SIZE 16

class TextureManager {
	private:
		GLuint _textureArrayID = 0;
	public:
		TextureManager();
		~TextureManager();
		void loadTexturesArray(std::vector<std::pair<TextureType, std::string>> data);
		GLuint getTextureArray() const;
};
