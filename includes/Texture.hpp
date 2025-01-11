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
    void addVertex(e_direction dir, int x, int y, int z);
    void processVertex(e_direction dir);
    void resetVertex();
    void setupBuffers(e_direction dir);
    int display(e_direction dir);
    GLuint getTexture(void);
private:
    void addVAOVertex(e_direction dir, int x, int y, int z, int u, int v);
    void processUpVertex();
    void processDownVertex();
    void processNorthVertex();
    void processSouthVertex();
    void processEastVertex();
    void processWestVertex();
};
