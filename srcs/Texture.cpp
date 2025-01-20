#include "Texture.hpp"
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <vector>

bool compareUpFaces(const Texture::Face& a, const Texture::Face& b) {
    if (a.resolution != b.resolution)
        return (a.resolution < b.resolution);
    if (a.position.y != b.position.y)
        return a.position.y < b.position.y;
    if (a.position.x != b.position.x)
        return a.position.x < b.position.x;
    return a.position.z < b.position.z;
}

bool compareUpStep2Faces(const Texture::Face& a, const Texture::Face& b) {
    if (a.resolution != b.resolution)
        return (a.resolution < b.resolution);
    if (a.position.y != b.position.y)
        return a.position.y < b.position.y;
    if (a.position.z != b.position.z)
        return a.position.z < b.position.z;
    return a.position.x < b.position.x;
}

bool compareNorthFaces(const Texture::Face& a, const Texture::Face& b) {
    if (a.resolution != b.resolution)
        return (a.resolution < b.resolution);
    if (a.position.z != b.position.z)
        return a.position.z < b.position.z;
    if (a.position.y != b.position.y)
        return a.position.y < b.position.y;
    return a.position.x < b.position.x;
}

bool compareNorthStep2Faces(const Texture::Face& a, const Texture::Face& b) {
    if (a.resolution != b.resolution)
        return (a.resolution < b.resolution);
    if (a.position.z != b.position.z)
        return a.position.z < b.position.z;
    if (a.position.x != b.position.x)
        return a.position.x < b.position.x;
    return a.position.y < b.position.y;
}

bool compareEastFaces(const Texture::Face& a, const Texture::Face& b) {
    if (a.resolution != b.resolution)
        return (a.resolution < b.resolution);
    if (a.position.x != b.position.x)
        return a.position.x < b.position.x;
    if (a.position.z != b.position.z)
        return a.position.z < b.position.z;
    return a.position.y < b.position.y;
}

bool compareEastStep2Faces(const Texture::Face& a, const Texture::Face& b) {
    if (a.resolution != b.resolution)
        return (a.resolution < b.resolution);
    if (a.position.x != b.position.x)
        return a.position.x < b.position.x;
    if (a.position.y != b.position.y)
        return a.position.y < b.position.y;
    return a.position.z < b.position.z;
}

Texture::Texture(GLuint texture) : _texture(texture) {
    // Initialize VAO and VBO
    glGenVertexArrays(1, &_vao);
    glGenBuffers(1, &_vbo);
}

Texture::~Texture() {
    resetVertex();
    if (_vao)
        glDeleteVertexArrays(1, &_vao);
    if (_vbo)
        glDeleteBuffers(1, &_vbo);
    if (_texture)
        glDeleteTextures(1, &_texture);
}

void Texture::processVertex()
{
    processUpVertex();
    processDownVertex();
    processNorthVertex();
    processSouthVertex();
    processEastVertex();
    processWestVertex();
}

void Texture::addVAOVertex(int x, int y, int z, int u, int v)
{
    _vertexData.push_back(x);
    _vertexData.push_back(y);
    _vertexData.push_back(z);
    _vertexData.push_back(u);
    _vertexData.push_back(v);
}

void Texture::processUpVertex()
{
    std::sort(_faces[UP].begin(), _faces[UP].end(), compareUpFaces);
    std::vector<Face> mergedFacesZ;
    std::vector<Face> mergedFaces;

    bool isFirst = true;
    Face lastFace;
    Face newFace;
    for (Face face : _faces[UP])
    {
        if (isFirst || face.position.x != newFace.position.x || face.position.y != newFace.position.y || lastFace.position.z != face.position.z - 1 || face.resolution != newFace.resolution)
        {
            if (!isFirst)
                mergedFacesZ.push_back(newFace);
            isFirst = false;
            newFace = Face(face);
        }
        newFace.size.y++;
        lastFace = Face(face);
    }
    mergedFacesZ.push_back(newFace);


    std::sort(mergedFacesZ.begin(), mergedFacesZ.end(), compareUpStep2Faces);
    isFirst = true;
    for (Face face : mergedFacesZ)
    {
        if (isFirst || face.position.y != newFace.position.y || face.position.z != newFace.position.z || lastFace.position.x != face.position.x - 1 || newFace.size.y != face.size.y || face.resolution != newFace.resolution)
        {
            if (!isFirst)
                mergedFaces.push_back(newFace);
            isFirst = false;
            newFace = Face(face);
        }
        newFace.size.x++;
        lastFace = Face(face);
    }
    mergedFaces.push_back(newFace);

    for (Face face : mergedFaces) {
        addVAOVertex(face.position.x                , face.position.y + face.resolution, face.position.z + face.size.y, 0, face.size.y / face.resolution);
        addVAOVertex(face.position.x + face.size.x  , face.position.y + face.resolution, face.position.z + face.size.y, face.size.x / face.resolution, face.size.y / face.resolution);
        addVAOVertex(face.position.x                , face.position.y + face.resolution, face.position.z              , 0, 0);

        addVAOVertex(face.position.x + face.size.x  , face.position.y + face.resolution, face.position.z + face.size.y, face.size.x / face.resolution, face.size.y / face.resolution);
        addVAOVertex(face.position.x + face.size.x  , face.position.y + face.resolution, face.position.z              , face.size.x / face.resolution, 0);
        addVAOVertex(face.position.x                , face.position.y + face.resolution, face.position.z              , 0, 0);
    }
}

void Texture::processDownVertex()
{
    std::sort(_faces[DOWN].begin(), _faces[DOWN].end(), compareUpFaces);
    std::vector<Face> mergedFacesZ;
    std::vector<Face> mergedFaces;

    bool isFirst = true;
    Face lastFace;
    Face newFace;
    for (Face face : _faces[DOWN])
    {
        if (isFirst || face.position.x != newFace.position.x || face.position.y != newFace.position.y || lastFace.position.z != face.position.z - 1 || face.resolution != newFace.resolution)
        {
            if (!isFirst)
                mergedFacesZ.push_back(newFace);
            isFirst = false;
            newFace = Face(face);
        }
        newFace.size.y++;
        lastFace = Face(face);
    }
    mergedFacesZ.push_back(newFace);

    std::sort(mergedFacesZ.begin(), mergedFacesZ.end(), compareUpStep2Faces);
    isFirst = true;
    for (Face face : mergedFacesZ)
    {
        if (isFirst || face.position.y != newFace.position.y || face.position.z != newFace.position.z || lastFace.position.x != face.position.x - 1 || newFace.size.y != face.size.y || face.resolution != newFace.resolution)
        {
            if (!isFirst)
                mergedFaces.push_back(newFace);
            isFirst = false;
            newFace = Face(face);
        }
        newFace.size.x++;
        lastFace = Face(face);
    }
    mergedFaces.push_back(newFace);

    for (Face face : mergedFaces) {
        addVAOVertex(face.position.x                , face.position.y, face.position.z              , 0, 0);
        addVAOVertex(face.position.x + face.size.x  , face.position.y, face.position.z + face.size.y, face.size.x / face.resolution, face.size.y / face.resolution);
        addVAOVertex(face.position.x                , face.position.y, face.position.z + face.size.y, 0, face.size.y / face.resolution);

        addVAOVertex(face.position.x                , face.position.y, face.position.z              , 0, 0);
        addVAOVertex(face.position.x + face.size.x  , face.position.y, face.position.z              , face.size.x / face.resolution, 0);
        addVAOVertex(face.position.x + face.size.x  , face.position.y, face.position.z + face.size.y, face.size.x / face.resolution, face.size.y / face.resolution);
    }
}

void Texture::processNorthVertex()
{
    std::sort(_faces[NORTH].begin(), _faces[NORTH].end(), compareNorthFaces);
    std::vector<Face> mergedFacesZ;
    std::vector<Face> mergedFaces;

    bool isFirst = true;
    Face lastFace;
    Face newFace;
    for (Face face : _faces[NORTH])
    {
        if (isFirst || face.position.y != newFace.position.y || face.position.z != newFace.position.z || lastFace.position.x != face.position.x - 1 || face.resolution != newFace.resolution)
        {
            if (!isFirst)
                mergedFacesZ.push_back(newFace);
            isFirst = false;
            newFace = Face(face);
        }
        newFace.size.x++;
        lastFace = Face(face);
    }
    mergedFacesZ.push_back(newFace);

    std::sort(mergedFacesZ.begin(), mergedFacesZ.end(), compareNorthStep2Faces);
    isFirst = true;
    for (Face face : mergedFacesZ)
    {
        if (isFirst || face.position.x != newFace.position.x || face.position.z != newFace.position.z || lastFace.position.y != face.position.y - 1 || newFace.size.x != face.size.x || face.resolution != newFace.resolution)
        {
            if (!isFirst)
                mergedFaces.push_back(newFace);
            isFirst = false;
            newFace = Face(face);
        }
        newFace.size.y++;
        lastFace = Face(face);
    }
    mergedFaces.push_back(newFace);

    for (Face face : mergedFaces) {
        addVAOVertex(face.position.x + face.size.x   , face.position.y + face.size.y , face.position.z, face.size.x / face.resolution, face.size.y / face.resolution);
        addVAOVertex(face.position.x + face.size.x   , face.position.y               , face.position.z, face.size.x / face.resolution, 0.0f);
        addVAOVertex(face.position.x                 , face.position.y               , face.position.z, 0.0f, 0.0f);

        addVAOVertex(face.position.x                 , face.position.y + face.size.y , face.position.z, 0.0f, face.size.y / face.resolution);
        addVAOVertex(face.position.x + face.size.x   , face.position.y + face.size.y , face.position.z, face.size.x / face.resolution, face.size.y / face.resolution);
        addVAOVertex(face.position.x                 , face.position.y               , face.position.z, 0.0f, 0.0f);
    }
}

void Texture::processSouthVertex()
{
    std::sort(_faces[SOUTH].begin(), _faces[SOUTH].end(), compareNorthFaces);
    std::vector<Face> mergedFacesZ;
    std::vector<Face> mergedFaces;

    bool isFirst = true;
    Face lastFace;
    Face newFace;
    for (Face face : _faces[SOUTH])
    {
        if (isFirst || face.position.y != newFace.position.y || face.position.z != newFace.position.z || lastFace.position.x != face.position.x - 1 || face.resolution != newFace.resolution)
        {
            if (!isFirst)
                mergedFacesZ.push_back(newFace);
            isFirst = false;
            newFace = Face(face);
        }
        newFace.size.x++;
        lastFace = Face(face);
    }
    mergedFacesZ.push_back(newFace);

    std::sort(mergedFacesZ.begin(), mergedFacesZ.end(), compareNorthStep2Faces);
    isFirst = true;
    for (Face face : mergedFacesZ)
    {
        if (isFirst || face.position.x != newFace.position.x || face.position.z != newFace.position.z || lastFace.position.y != face.position.y - 1 || newFace.size.x != face.size.x || face.resolution != newFace.resolution)
        {
            if (!isFirst)
                mergedFaces.push_back(newFace);
            isFirst = false;
            newFace = Face(face);
        }
        newFace.size.y++;
        lastFace = Face(face);
    }
    mergedFaces.push_back(newFace);

    for (Face face : mergedFaces) {
        addVAOVertex(face.position.x                 , face.position.y               , face.position.z + face.resolution, 0.0f, 0.0f);
        addVAOVertex(face.position.x + face.size.x   , face.position.y               , face.position.z + face.resolution, face.size.x / face.resolution, 0.0f);
        addVAOVertex(face.position.x + face.size.x   , face.position.y + face.size.y , face.position.z + face.resolution, face.size.x / face.resolution, face.size.y / face.resolution);

        addVAOVertex(face.position.x                 , face.position.y               , face.position.z + face.resolution, 0.0f, 0.0f);
        addVAOVertex(face.position.x + face.size.x   , face.position.y + face.size.y , face.position.z + face.resolution, face.size.x / face.resolution, face.size.y / face.resolution);
        addVAOVertex(face.position.x                 , face.position.y + face.size.y , face.position.z + face.resolution, 0.0f, face.size.y / face.resolution);
    }
}

void Texture::processEastVertex()
{
    std::sort(_faces[EAST].begin(), _faces[EAST].end(), compareEastFaces);
    std::vector<Face> mergedFacesZ;
    std::vector<Face> mergedFaces;

    bool isFirst = true;
    Face lastFace;
    Face newFace;
    for (Face face : _faces[EAST])
    {
        if (isFirst || face.position.z != newFace.position.z || face.position.x != newFace.position.x || lastFace.position.y != face.position.y - 1 || face.resolution != newFace.resolution)
        {
            if (!isFirst)
                mergedFacesZ.push_back(newFace);
            isFirst = false;
            newFace = Face(face);
        }
        newFace.size.x++;
        lastFace = Face(face);
    }
    mergedFacesZ.push_back(newFace);
    std::sort(mergedFacesZ.begin(), mergedFacesZ.end(), compareEastStep2Faces);
    isFirst = true;
    for (Face face : mergedFacesZ)
    {
        if (isFirst || face.position.x != newFace.position.x || face.position.y != newFace.position.y || lastFace.position.z != face.position.z - 1 || newFace.size.x != face.size.x || face.resolution != newFace.resolution)
        {
            if (!isFirst)
                mergedFaces.push_back(newFace);
            isFirst = false;
            newFace = Face(face);
        }
        newFace.size.y++;
        lastFace = Face(face);
    }
    mergedFaces.push_back(newFace);

    for (Face face : mergedFaces) {
        addVAOVertex(face.position.x, face.position.y, face.position.z, 0, 0);
        addVAOVertex(face.position.x, face.position.y, face.position.z + face.size.y, face.size.y / face.resolution, 0);
        addVAOVertex(face.position.x, face.position.y + face.size.x, face.position.z + face.size.y, face.size.y / face.resolution, face.size.x / face.resolution);

        addVAOVertex(face.position.x, face.position.y, face.position.z, 0, 0);
        addVAOVertex(face.position.x, face.position.y + face.size.x, face.position.z + face.size.y, face.size.y / face.resolution, face.size.x / face.resolution);
        addVAOVertex(face.position.x, face.position.y + face.size.x, face.position.z, 0, face.size.x / face.resolution);
    }
}

void Texture::processWestVertex()
{
    std::sort(_faces[WEST].begin(), _faces[WEST].end(), compareEastFaces);
    std::vector<Face> mergedFacesZ;
    std::vector<Face> mergedFaces;

    bool isFirst = true;
    Face lastFace;
    Face newFace;
    for (Face face : _faces[WEST])
    {
        if (isFirst || face.position.z != newFace.position.z || face.position.x != newFace.position.x || lastFace.position.y != face.position.y - 1 || face.resolution != newFace.resolution)
        {
            if (!isFirst)
                mergedFacesZ.push_back(newFace);
            isFirst = false;
            newFace = Face(face);
        }
        newFace.size.x++;
        lastFace = Face(face);
    }
    mergedFacesZ.push_back(newFace);
    std::sort(mergedFacesZ.begin(), mergedFacesZ.end(), compareEastStep2Faces);
    isFirst = true;
    for (Face face : mergedFacesZ)
    {
        if (isFirst || face.position.x != newFace.position.x || face.position.y != newFace.position.y || lastFace.position.z != face.position.z - 1 || newFace.size.x != face.size.x || face.resolution != newFace.resolution)
        {
            if (!isFirst)
                mergedFaces.push_back(newFace);
            isFirst = false;
            newFace = Face(face);
        }
        newFace.size.y++;
        lastFace = Face(face);
    }
    mergedFaces.push_back(newFace);

    for (Face face : mergedFaces) {
        addVAOVertex(face.position.x + face.resolution, face.position.y + face.size.x, face.position.z + face.size.y, face.size.y / face.resolution, face.size.x / face.resolution);
        addVAOVertex(face.position.x + face.resolution, face.position.y, face.position.z + face.size.y, face.size.y / face.resolution, 0);
        addVAOVertex(face.position.x + face.resolution, face.position.y, face.position.z, 0, 0);

        addVAOVertex(face.position.x + face.resolution, face.position.y + face.size.x, face.position.z, 0, face.size.x / face.resolution);
        addVAOVertex(face.position.x + face.resolution, face.position.y + face.size.x, face.position.z + face.size.y, face.size.y / face.resolution, face.size.x / face.resolution);
        addVAOVertex(face.position.x + face.resolution, face.position.y, face.position.z, 0, 0);
    }
}

void Texture::addVertex(e_direction dir, int x, int y, int z, int resolution) {
    Face newFace;
    newFace.position = vec3(x, y, z);
    newFace.size = vec2(resolution - 1, resolution - 1);
    newFace.resolution = resolution;
    _faces[dir].push_back(newFace);
}

void Texture::resetVertex() {
    for (int i = 0; i < 6; i++)
        _faces[i].clear();
    _vertexData.clear();
}

void Texture::setupBuffers() {
    if (_vertexData.empty()) return;

    glBindVertexArray(_vao);

    glBindBuffer(GL_ARRAY_BUFFER, _vbo);
    glBufferData(GL_ARRAY_BUFFER, _vertexData.size() * sizeof(float), _vertexData.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0); // Positions
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float))); // Texture coords
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

int Texture::display(void) {
    if (_vertexData.size() % 5 != 0) {
        std::cerr << "Couldn't display because array is not divisible by 5. Found " << _vertexData.size() << std::endl;
        return 0;
    }
    if (_vertexData.size() == 0)
        return 0;
    setupBuffers();
    glBindTexture(GL_TEXTURE_2D, _texture);

    glBindVertexArray(_vao);
    glDrawArrays(GL_TRIANGLES, 0, _vertexData.size() / 5); // 5 floats per vertex
    glBindVertexArray(0);

    glBindTexture(GL_TEXTURE_2D, 0);
	return ((_vertexData.size() / 15));
}

GLuint Texture::getTexture() {
    return _texture;
}
