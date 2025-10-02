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
};

# define N_TEXTURES 12
# define TEXTURE_SIZE 16

class TextureManager {
	private:
		GLuint _textureArrayID = 0;
        glm::vec3 _avgColors[N_TEXTURES];
        std::vector<glm::vec3> _palettes[N_TEXTURES]; // per-texture RGB palette (non-transparent)
	public:
		TextureManager();
		~TextureManager();
		void loadTexturesArray(std::vector<std::pair<TextureType, std::string>> data);
		GLuint getTextureArray() const;
        glm::vec3 getAverageColor(TextureType t) const { return _avgColors[(int)t]; }
        // Sample N random colors from the texture palette (fallback to average if empty)
        std::vector<glm::vec3> sampleColors(TextureType t, int count, std::mt19937 &rng) const;
};
