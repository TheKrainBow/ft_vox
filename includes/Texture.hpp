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
        int       resolution;
    } Face;
private:
    std::vector<Face>	_faces[6];
    GLuint				_texture;
    GLuint				_vao;
    GLuint				_vbo;
    std::vector<float>	_vertexData;
    int					_width, _height;

public:
    Texture(GLuint texture);
    ~Texture();
    void addVertex(e_direction dir, int x, int y, int z, int textureSize);
    void processVertex(void);
    void resetVertex();
    void setupBuffers(void);
    int display(void);
    GLuint getTexture(void);
private:
    void addVAOVertex(int x, int y, int z, int u, int v);
    void processUpVertex();
    void processDownVertex();
    void processNorthVertex();
    void processSouthVertex();
    void processEastVertex();
    void processWestVertex();
};
