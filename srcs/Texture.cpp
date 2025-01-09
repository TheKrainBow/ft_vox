#include "Texture.hpp"
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <vector>

Texture::Texture(GLuint texture) : _texture(texture), _vao(0), _vbo(0) {
    // Initialize VAO and VBO
    glGenVertexArrays(1, &_vao);
    glGenBuffers(1, &_vbo);
}

Texture::~Texture() {
    resetVertex();
    if (_vao) {
        glDeleteVertexArrays(1, &_vao);
    }
    if (_vbo) {
        glDeleteBuffers(1, &_vbo);
    }
    if (_texture) {
        glDeleteTextures(1, &_texture);
    }
}

void Texture::addVertex(int x, int y, int z, double u, double v) {
    _vertexData.push_back(x);
    _vertexData.push_back(y);
    _vertexData.push_back(z);
    // Add texture coordinates as well
    _vertexData.push_back(u); // texCoordX
    _vertexData.push_back(v); // texCoordY
}

void Texture::resetVertex() {
    _vertexData.clear();
}

void Texture::setupBuffers() {
    if (_vertexData.empty()) return;

    // Bind the VAO
    glBindVertexArray(_vao);

    // Bind the VBO and upload the vertex data
    glBindBuffer(GL_ARRAY_BUFFER, _vbo);
    glBufferData(GL_ARRAY_BUFFER, _vertexData.size() * sizeof(float), _vertexData.data(), GL_STATIC_DRAW);

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

void Texture::display() {
    if (_vertexData.size() % 5 != 0) {
        std::cerr << "Couldn't display because array is not divisible by 5. Found " << _vertexData.size() << std::endl;
        return;
    }

    // Ensure the VAO is set up
    setupBuffers();
    // Use the texture
	//glActiveTexture(GL_TEXTURE0); 
    glBindTexture(GL_TEXTURE_2D, _texture);

    // Bind the VAO and draw the object
    glBindVertexArray(_vao);
    glDrawArrays(GL_TRIANGLES, 0, _vertexData.size() / 5); // 5 floats per vertex
    glBindVertexArray(0);

    // Unbind the texture
    glBindTexture(GL_TEXTURE_2D, 0);
}

GLuint Texture::getTexture() {
    return _texture;
}
