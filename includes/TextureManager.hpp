#pragma once

#include "ft_vox.hpp"

struct t_rgb {
    unsigned char r;
    unsigned char g;
    unsigned char b;
    t_rgb(unsigned char r, unsigned char g, unsigned char b) : r(r), g(g), b(b) {}
    t_rgb() : r(0), g(0), b(0) {}
};

enum TextureType {
    DIRT,
    COBBLE
};

class TextureManager {
private:
    std::map<TextureType, GLuint> _textures;
public:
    TextureManager(); // Constructor declaration
    ~TextureManager(); // Destructor declaration

    void addTexture(TextureType type, std::string path);
    GLuint getTexture(TextureType type);
    
    // Declaring external functions that are not part of the class
    t_rgb *loadPPM(const std::string &path, int &width, int &height);
    GLuint loadTexture(const std::string& filename);
};