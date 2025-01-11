#include "Texture.hpp"
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <vector>

bool compareUpFaces(const Texture::Face& a, const Texture::Face& b) {
    if (a.position.y != b.position.y)
        return a.position.y < b.position.y;
    if (a.position.x != b.position.x)
        return a.position.x < b.position.x;
    return a.position.z < b.position.z;
}

bool compareUpStep2Faces(const Texture::Face& a, const Texture::Face& b) {
    if (a.position.y != b.position.y)
        return a.position.y < b.position.y;
    if (a.position.z != b.position.z)
        return a.position.z < b.position.z;
    return a.position.x < b.position.x;
}

bool compareNorthFaces(const Texture::Face& a, const Texture::Face& b) {
    if (a.position.z != b.position.z)
        return a.position.z < b.position.z;
    if (a.position.y != b.position.y)
        return a.position.y < b.position.y;
    return a.position.x < b.position.x;
}

bool compareNorthStep2Faces(const Texture::Face& a, const Texture::Face& b) {
    if (a.position.z != b.position.z)
        return a.position.z < b.position.z;
    if (a.position.x != b.position.x)
        return a.position.x < b.position.x;
    return a.position.y < b.position.y;
}

bool compareEastFaces(const Texture::Face& a, const Texture::Face& b) {
    if (a.position.x != b.position.x)
        return a.position.x < b.position.x;
    if (a.position.z != b.position.z)
        return a.position.z < b.position.z;
    return a.position.y < b.position.y;
}

bool compareEastStep2Faces(const Texture::Face& a, const Texture::Face& b) {
    if (a.position.x != b.position.x)
        return a.position.x < b.position.x;
    if (a.position.y != b.position.y)
        return a.position.y < b.position.y;
    return a.position.z < b.position.z;
}

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

void Texture::processVertex(e_direction dir)
{
    if (_faces[dir].size() == 0)
        return ;
    switch (dir)
    {
    case UP:
        this->processUpVertex();
        break;
    case DOWN:
        this->processDownVertex();
        break;
    case NORTH:
        this->processNorthVertex();
        break;
    case SOUTH:
        this->processSouthVertex();
        break;
    case EAST:
        this->processEastVertex();
        break;
    case WEST:
        this->processWestVertex();
        break;
    default:
        break;
    }
}

void Texture::addVAOVertex(e_direction dir, int x, int y, int z, int u, int v)
{
    _vertexData[dir].push_back(x);
    _vertexData[dir].push_back(y);
    _vertexData[dir].push_back(z);
    _vertexData[dir].push_back(u);
    _vertexData[dir].push_back(v);
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
        if (isFirst || face.position.x != newFace.position.x || face.position.y != newFace.position.y || lastFace.position.z != face.position.z - 1)
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
        if (isFirst || face.position.y != newFace.position.y || face.position.z != newFace.position.z || lastFace.position.x != face.position.x - 1 || newFace.size.y != face.size.y)
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
        addVAOVertex(UP, face.position.x                , face.position.y + 1, face.position.z + face.size.y, 0, face.size.y);
        addVAOVertex(UP, face.position.x + face.size.x  , face.position.y + 1, face.position.z + face.size.y, face.size.x, face.size.y);
        addVAOVertex(UP, face.position.x                , face.position.y + 1, face.position.z              , 0, 0);

        addVAOVertex(UP, face.position.x + face.size.x  , face.position.y + 1, face.position.z + face.size.y, face.size.x, face.size.y);
        addVAOVertex(UP, face.position.x + face.size.x  , face.position.y + 1, face.position.z              , face.size.x, 0);
        addVAOVertex(UP, face.position.x                , face.position.y + 1, face.position.z              , 0, 0);
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
        if (isFirst || face.position.x != newFace.position.x || face.position.y != newFace.position.y || lastFace.position.z != face.position.z - 1)
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
        if (isFirst || face.position.y != newFace.position.y || face.position.z != newFace.position.z || lastFace.position.x != face.position.x - 1 || newFace.size.y != face.size.y)
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
        addVAOVertex(DOWN, face.position.x                , face.position.y, face.position.z              , 0, 0);
        addVAOVertex(DOWN, face.position.x + face.size.x  , face.position.y, face.position.z + face.size.y, face.size.x, face.size.y);
        addVAOVertex(DOWN, face.position.x                , face.position.y, face.position.z + face.size.y, 0, face.size.y);

        addVAOVertex(DOWN, face.position.x                , face.position.y, face.position.z              , 0, 0);
        addVAOVertex(DOWN, face.position.x + face.size.x  , face.position.y, face.position.z              , face.size.x, 0);
        addVAOVertex(DOWN, face.position.x + face.size.x  , face.position.y, face.position.z + face.size.y, face.size.x, face.size.y);
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
        if (isFirst || face.position.y != newFace.position.y || face.position.z != newFace.position.z || lastFace.position.x != face.position.x - 1)
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
        if (isFirst || face.position.x != newFace.position.x || face.position.z != newFace.position.z || lastFace.position.y != face.position.y - 1 || newFace.size.x != face.size.x)
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
        addVAOVertex(NORTH, face.position.x + face.size.x   , face.position.y + face.size.y , face.position.z, face.size.x, face.size.y);
        addVAOVertex(NORTH, face.position.x + face.size.x   , face.position.y               , face.position.z, face.size.x, 0.0f);
        addVAOVertex(NORTH, face.position.x                 , face.position.y               , face.position.z, 0.0f, 0.0f);

        addVAOVertex(NORTH, face.position.x                 , face.position.y + face.size.y , face.position.z, 0.0f, face.size.y);
        addVAOVertex(NORTH, face.position.x + face.size.x   , face.position.y + face.size.y , face.position.z, face.size.x, face.size.y);
        addVAOVertex(NORTH, face.position.x                 , face.position.y               , face.position.z, 0.0f, 0.0f);
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
        if (isFirst || face.position.y != newFace.position.y || face.position.z != newFace.position.z || lastFace.position.x != face.position.x - 1)
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
        if (isFirst || face.position.x != newFace.position.x || face.position.z != newFace.position.z || lastFace.position.y != face.position.y - 1 || newFace.size.x != face.size.x)
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
        addVAOVertex(SOUTH, face.position.x                 , face.position.y               , face.position.z + 1, 0.0f, 0.0f);
        addVAOVertex(SOUTH, face.position.x + face.size.x   , face.position.y               , face.position.z + 1, face.size.x, 0.0f);
        addVAOVertex(SOUTH, face.position.x + face.size.x   , face.position.y + face.size.y , face.position.z + 1, face.size.x, face.size.y);

        addVAOVertex(SOUTH, face.position.x                 , face.position.y               , face.position.z + 1, 0.0f, 0.0f);
        addVAOVertex(SOUTH, face.position.x + face.size.x   , face.position.y + face.size.y , face.position.z + 1, face.size.x, face.size.y);
        addVAOVertex(SOUTH, face.position.x                 , face.position.y + face.size.y , face.position.z + 1, 0.0f, face.size.y);
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
        if (isFirst || face.position.z != newFace.position.z || face.position.x != newFace.position.x || lastFace.position.y != face.position.y - 1)
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
        if (isFirst || face.position.x != newFace.position.x || face.position.y != newFace.position.y || lastFace.position.z != face.position.z - 1 || newFace.size.x != face.size.x)
        {
            if (!isFirst)
                mergedFaces.push_back(newFace);
            isFirst = false;
            newFace = Face(face);
        }
        //std::cout << " Newf: " << newFace.position.x << ", " << newFace.position.y << ", " << newFace.position.z << " (" << newFace.size.y << ", " << newFace.size.x << ")" << std::endl;
        //std::cout << "Face: " << face.position.x << ", " << face.position.y << ", " << face.position.z << " (" << face.size.y << ", " << face.size.x << ")" << std::endl;
        //std::cout << "Last: " << lastFace.position.x << ", " << lastFace.position.y << ", " << lastFace.position.z << " (" << lastFace.size.y << ", " << lastFace.size.x << ")" << std::endl;
        newFace.size.y++;
        lastFace = Face(face);
    }
    mergedFaces.push_back(newFace);

    for (Face face : mergedFaces) {
        addVAOVertex(EAST, face.position.x, face.position.y, face.position.z, 0, 0);
        addVAOVertex(EAST, face.position.x, face.position.y, face.position.z + face.size.y, face.size.y, 0);
        addVAOVertex(EAST, face.position.x, face.position.y + face.size.x, face.position.z + face.size.y, face.size.y, face.size.x);

        addVAOVertex(EAST, face.position.x, face.position.y, face.position.z, 0, 0);
        addVAOVertex(EAST, face.position.x, face.position.y + face.size.x, face.position.z + face.size.y, face.size.y, face.size.x);
        addVAOVertex(EAST, face.position.x, face.position.y + face.size.x, face.position.z, 0, face.size.x);
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
        if (isFirst || face.position.z != newFace.position.z || face.position.x != newFace.position.x || lastFace.position.y != face.position.y - 1)
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
        if (isFirst || face.position.x != newFace.position.x || face.position.y != newFace.position.y || lastFace.position.z != face.position.z - 1 || newFace.size.x != face.size.x)
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
        addVAOVertex(WEST, face.position.x + 1, face.position.y + face.size.x, face.position.z + face.size.y, face.size.y, face.size.x);
        addVAOVertex(WEST, face.position.x + 1, face.position.y, face.position.z + face.size.y, face.size.y, 0);
        addVAOVertex(WEST, face.position.x + 1, face.position.y, face.position.z, 0, 0);

        addVAOVertex(WEST, face.position.x + 1, face.position.y + face.size.x, face.position.z, 0, face.size.x);
        addVAOVertex(WEST, face.position.x + 1, face.position.y + face.size.x, face.position.z + face.size.y, face.size.y, face.size.x);
        addVAOVertex(WEST, face.position.x + 1, face.position.y, face.position.z, 0, 0);
    }
}

void Texture::addVertex(e_direction dir, int x, int y, int z) {
    Face newFace;

    newFace.position = vec3(x, y, z);
    newFace.size = vec2(0, 0);
    _faces[dir].push_back(newFace);
}

void Texture::resetVertex() {
    for (int i = 0; i < 6; i++)
    {
        _vertexData[i].clear();
        _faces[i].clear();
    }
}

void Texture::setupBuffers(e_direction dir) {
    if (_vertexData[dir].empty()) return;

    glBindVertexArray(_vao[dir]);

    glBindBuffer(GL_ARRAY_BUFFER, _vbo[dir]);
    glBufferData(GL_ARRAY_BUFFER, _vertexData[dir].size() * sizeof(float), _vertexData[dir].data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0); // Positions
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float))); // Texture coords
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

int Texture::display(e_direction dir) {
    if (_vertexData[dir].size() % 5 != 0) {
        std::cerr << "Couldn't display because array is not divisible by 5. Found " << _vertexData[dir].size() << std::endl;
        return 0;
    }
    setupBuffers(dir);
    glBindTexture(GL_TEXTURE_2D, _texture);

    glBindVertexArray(_vao[dir]);
    glDrawArrays(GL_TRIANGLES, 0, _vertexData[dir].size() / 5); // 5 floats per vertex
    glBindVertexArray(0);

    glBindTexture(GL_TEXTURE_2D, 0);
	return ((_vertexData[dir].size() / 15));
}

GLuint Texture::getTexture() {
    return _texture;
}
