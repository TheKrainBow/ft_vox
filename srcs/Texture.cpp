#include "Texture.hpp"
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <vector>

Texture::Texture(GLuint texture) : _texture(texture) {
    // Initialize VAO and VBO
    for (int i = 0; i < 6; i++)
    {
        glGenVertexArrays(1, &_vao[i]);
        glGenBuffers(1, &_vbo[i]);
    }
}

Texture::~Texture() {
    resetVertex();
    for (int i = 0; i < 6; i++)
    {
        if (_vao[i])
            glDeleteVertexArrays(1, &_vao[i]);
        if (_vbo[i])
            glDeleteBuffers(1, &_vbo[i]);
    }
    if (_texture)
        glDeleteTextures(1, &_texture);
}

void Texture::addVertex(e_direction dir, int x, int y, int z, double u, double v) {
    _vertexData[dir].push_back(x);
    _vertexData[dir].push_back(y);
    _vertexData[dir].push_back(z);
    // Add texture coordinates as well
    _vertexData[dir].push_back(u); // texCoordX
    _vertexData[dir].push_back(v); // texCoordY
}

void Texture::resetVertex() {
    for (int i = 0; i < 6; i++)
        _vertexData[i].clear();
}

void Texture::setupBuffers(e_direction dir) {
    if (_vertexData[dir].empty()) return;

    // Bind the VAO
    glBindVertexArray(_vao[dir]);

    // Bind the VBO and upload the vertex data
    glBindBuffer(GL_ARRAY_BUFFER, _vbo[dir]);
    glBufferData(GL_ARRAY_BUFFER, _vertexData[dir].size() * sizeof(float), _vertexData[dir].data(), GL_STATIC_DRAW);

    // Set up position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0); // Positions
    glEnableVertexAttribArray(0);

    // Set up texture coordinate attribute
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float))); // Texture coords
    glEnableVertexAttribArray(1);

    // Unbind VAO and VBO
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

int Texture::display(e_direction dir) {
    if (_vertexData[dir].size() % 5 != 0) {
        std::cerr << "Couldn't display because array is not divisible by 5. Found " << _vertexData[dir].size() << std::endl;
        return 0;
    }

    // Ensure the VAO is set up
    setupBuffers(dir);
    // Use the texture
	//glActiveTexture(GL_TEXTURE0); 
    glBindTexture(GL_TEXTURE_2D, _texture);

    // Bind the VAO and draw the object
    glBindVertexArray(_vao[dir]);
    glDrawArrays(GL_TRIANGLES, 0, _vertexData[dir].size() / 5); // 5 floats per vertex
    glBindVertexArray(0);

    // Unbind the texture
    glBindTexture(GL_TEXTURE_2D, 0);
	return ((_vertexData[dir].size() / 5) * 3);
}

GLuint Texture::getTexture() {
    return _texture;
}
