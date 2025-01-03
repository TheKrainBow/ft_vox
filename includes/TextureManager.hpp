#pragma once

#include "ft_vox.hpp"

#define NB_TEXTURES 2

struct t_rgb {
	uint8_t r, g, b;
};

class TextureManager {
private:
    GLuint _textures[NB_TEXTURES];
public:
    TextureManager(); // Constructor declaration
    ~TextureManager(); // Destructor declaration

    void addTexture(BlockType type, std::string path);
    GLuint getTexture(BlockType type);
    
    // Declaring external functions that are not part of the class
    t_rgb *loadPPM(const std::string &path, int &width, int &height);
    GLuint loadTexture(const std::string& filename);
};
