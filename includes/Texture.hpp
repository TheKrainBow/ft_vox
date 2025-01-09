#pragma once

#include "ft_vox.hpp"
#include <vector>
#include <GL/glew.h>
#include <GLFW/glfw3.h>

class Texture
{
private:
    GLuint				_texture;
    GLuint				_vao;               // Vertex Array Object
    GLuint				_vbo;               // Vertex Buffer Object
    std::vector<float>	_vertexData;     // Flattened vertex data: [x, y, z, texCoordX, texCoordY]
    int					_width, _height;

public:
    Texture(GLuint texture);
    ~Texture();
    void addVertex(int x, int y, int z, double u, double v);
    void resetVertex(void);
    void setupBuffers(void);
    void display(void);
    GLuint getTexture(void);
};
