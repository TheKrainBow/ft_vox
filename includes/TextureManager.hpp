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
	// Mesh-only water variant; shaders sample T_WATER (no extra atlas layer).
	T_WATER_FULL = 14,
	T_LAVA = 15,
	T_LAVA_FULL = 16,
	T_LAVA_FLOW_1 = 17, T_LAVA_FLOW_2, T_LAVA_FLOW_3, T_LAVA_FLOW_4,
	T_LAVA_FLOW_5, T_LAVA_FLOW_6, T_LAVA_FLOW_7,
};

# define N_TEXTURES 14
# define TEXTURE_SIZE 16

class TextureManager {
	private:
		GLuint _textureArrayID = 0;
	public:
		TextureManager();
		~TextureManager();
		// Explicit GL teardown (safe to call multiple times)
		void shutdownGL();
		void loadTexturesArray(std::vector<std::pair<TextureType, std::string>> data);
		GLuint getTextureArray() const;
};
