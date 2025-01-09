#pragma once

#include "ft_vox.hpp"
#include <vector>
#include <GL/glew.h>
#include <GLFW/glfw3.h>

class Texture
{
private:
    GLuint				_texture;
    GLuint				_vao[6];               // Vertex Array Object
    GLuint				_vbo[6];               // Vertex Buffer Object
    std::vector<float>	_vertexData[6];     // Flattened vertex data: [x, y, z, texCoordX, texCoordY]
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
