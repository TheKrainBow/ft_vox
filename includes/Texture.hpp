#pragma once

#include "ft_vox.hpp"
#include <vector>
#include <GL/glew.h>
#include <GLFW/glfw3.h>

class Texture
{
private:
    GLuint				_texture;
    //
    GLuint				_vao[6];
    GLuint				_vbo[6];
    std::vector<float>	_vertexData[6];
    int					_width, _height;

public:
    Texture(GLuint texture);
    ~Texture();
    void addVertex(e_direction dir, int x, int y, int z, double u, double v);
    void resetVertex();
    void setupBuffers(e_direction dir);
    int display(e_direction dir);
    GLuint getTexture(void);
};
