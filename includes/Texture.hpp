#pragma once

#include "ft_vox.hpp"
#include <vector>
#include <GL/glew.h>
#include <GLFW/glfw3.h>

class Texture
{
public:
    typedef struct s_Face {
        glm::vec3 position;
        glm::vec2 size;
    } Face;
private:
    std::vector<Face>	_faces[6];
    GLuint				_texture;
    GLuint				_vao[6];
    GLuint				_vbo[6];
    std::vector<float>	_vertexData[6];
    int					_width, _height;

public:
    Texture(GLuint texture);
    ~Texture();
    void addVertex(Direction dir, int x, int y, int z);
    void processVertex(Direction dir);
    void resetVertex();
    void setupBuffers(Direction dir);
    int display(Direction dir);
    GLuint getTexture(void);
private:
    void addVAOVertex(Direction dir, int x, int y, int z, int u, int v);
    void processUpVertex();
    void processDownVertex();
    void processNorthVertex();
    void processSouthVertex();
    void processEastVertex();
    void processWestVertex();
};
