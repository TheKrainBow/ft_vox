#include "SubChunk.hpp"

bool compareUpFaces(const SubChunk::Face& a, const SubChunk::Face& b) {
    if (a.texture != b.texture)
        return (a.texture > b.texture);
    if (a.position.y != b.position.y)
        return a.position.y < b.position.y;
    if (a.position.x != b.position.x)
        return a.position.x < b.position.x;
    return a.position.z < b.position.z;
}

bool compareUpStep2Faces(const SubChunk::Face& a, const SubChunk::Face& b) {
    if (a.texture != b.texture)
        return (a.texture < b.texture);
    if (a.position.y != b.position.y)
        return a.position.y < b.position.y;
    if (a.position.z != b.position.z)
        return a.position.z < b.position.z;
    return a.position.x < b.position.x;
}

bool compareNorthFaces(const SubChunk::Face& a, const SubChunk::Face& b) {
    if (a.texture != b.texture)
        return (a.texture < b.texture);
    if (a.position.z != b.position.z)
        return a.position.z < b.position.z;
    if (a.position.y != b.position.y)
        return a.position.y < b.position.y;
    return a.position.x < b.position.x;
}

bool compareNorthStep2Faces(const SubChunk::Face& a, const SubChunk::Face& b) {
    if (a.texture != b.texture)
        return (a.texture < b.texture);
    if (a.position.z != b.position.z)
        return a.position.z < b.position.z;
    if (a.position.x != b.position.x)
        return a.position.x < b.position.x;
    return a.position.y < b.position.y;
}

bool compareEastFaces(const SubChunk::Face& a, const SubChunk::Face& b) {
    if (a.texture != b.texture)
        return (a.texture < b.texture);
    if (a.position.x != b.position.x)
        return a.position.x < b.position.x;
    if (a.position.z != b.position.z)
        return a.position.z < b.position.z;
    return a.position.y < b.position.y;
}

bool compareEastStep2Faces(const SubChunk::Face& a, const SubChunk::Face& b) {
    if (a.texture != b.texture)
        return (a.texture < b.texture);
    if (a.position.x != b.position.x)
        return a.position.x < b.position.x;
    if (a.position.y != b.position.y)
        return a.position.y < b.position.y;
    return a.position.z < b.position.z;
}


void SubChunk::processUpVertex(std::vector<Face> *faces, std::vector<int> *vertexData)
{
    if (faces[UP].empty())
        return ;
    std::sort(faces[UP].begin(), faces[UP].end(), compareUpFaces);
    std::vector<Face> mergedFacesZ;
    std::vector<Face> mergedFaces;

    bool isFirst = true;
    Face lastFace;
    Face newFace;
    for (Face face : faces[UP])
    {
        if (isFirst || newFace.size.y > 31 || newFace.texture != face.texture || face.position.x != newFace.position.x || face.position.y != newFace.position.y || lastFace.position.z != face.position.z - _resolution)
        {
            if (isFirst == false)
                mergedFacesZ.push_back(newFace);
            isFirst = false;
            newFace = Face(face);
        }
        newFace.size.y += _resolution;
        lastFace = Face(face);
    }
    mergedFacesZ.push_back(newFace);
    std::sort(mergedFacesZ.begin(), mergedFacesZ.end(), compareUpStep2Faces);
    isFirst = true;
    for (Face face : mergedFacesZ)
    {
        if (isFirst || newFace.size.x > 31 || face.texture != newFace.texture || face.position.y != newFace.position.y || face.position.z != newFace.position.z || lastFace.position.x != face.position.x - _resolution || newFace.size.y != face.size.y)
        {
            if (!isFirst)
                mergedFaces.push_back(newFace);
            isFirst = false;
            newFace = Face(face);
        }
        newFace.size.x += _resolution;
        lastFace = Face(face);
    }
    mergedFaces.push_back(newFace);

    for (Face face : mergedFaces) {
        addTextureVertex(face, vertexData);
    }
}

void SubChunk::processDownVertex(std::vector<Face> *faces, std::vector<int> *vertexData)
{
    if (faces[DOWN].empty())
        return ;
    std::sort(faces[DOWN].begin(), faces[DOWN].end(), compareUpFaces);
    std::vector<Face> mergedFacesZ;
    std::vector<Face> mergedFaces;

    bool isFirst = true;
    Face lastFace;
    Face newFace;
    for (Face face : faces[DOWN])
    {
        if (isFirst || newFace.size.y > 31 || face.texture != newFace.texture || face.position.x != newFace.position.x || face.position.y != newFace.position.y || lastFace.position.z != face.position.z - _resolution)
        {
            if (!isFirst)
                mergedFacesZ.push_back(newFace);
            isFirst = false;
            newFace = Face(face);
        }
        newFace.size.y += _resolution;
        lastFace = Face(face);
    }
    mergedFacesZ.push_back(newFace);

    std::sort(mergedFacesZ.begin(), mergedFacesZ.end(), compareUpStep2Faces);
    isFirst = true;
    for (Face face : mergedFacesZ)
    {
        if (isFirst || newFace.size.x > 31 || face.texture != newFace.texture || face.position.y != newFace.position.y || face.position.z != newFace.position.z || lastFace.position.x != face.position.x - _resolution || newFace.size.y != face.size.y)
        {
            if (!isFirst)
                mergedFaces.push_back(newFace);
            isFirst = false;
            newFace = Face(face);
        }
        newFace.size.x += _resolution;
        lastFace = Face(face);
    }
    mergedFaces.push_back(newFace);

    for (Face face : mergedFaces) {
        addTextureVertex(face, vertexData);
    }
}

void SubChunk::processNorthVertex(std::vector<Face> *faces, std::vector<int> *vertexData)
{
    if (faces[NORTH].empty())
        return ;
    std::sort(faces[NORTH].begin(), faces[NORTH].end(), compareNorthFaces);
    std::vector<Face> mergedFacesZ;
    std::vector<Face> mergedFaces;

    bool isFirst = true;
    Face lastFace;
    Face newFace;
    for (Face face : faces[NORTH])
    {
        if (isFirst || newFace.size.x > 31 || face.texture != newFace.texture || face.position.y != newFace.position.y || face.position.z != newFace.position.z || lastFace.position.x != face.position.x - _resolution)
        {
            if (!isFirst)
                mergedFacesZ.push_back(newFace);
            isFirst = false;
            newFace = Face(face);
        }
        newFace.size.x += _resolution;
        lastFace = Face(face);
    }
    mergedFacesZ.push_back(newFace);

    std::sort(mergedFacesZ.begin(), mergedFacesZ.end(), compareNorthStep2Faces);
    isFirst = true;
    for (Face face : mergedFacesZ)
    {
        if (isFirst || newFace.size.y > 31 || face.texture != newFace.texture || face.position.x != newFace.position.x || face.position.z != newFace.position.z || lastFace.position.y != face.position.y - _resolution || newFace.size.x != face.size.x)
        {
            if (!isFirst)
                mergedFaces.push_back(newFace);
            isFirst = false;
            newFace = Face(face);
        }
        newFace.size.y += _resolution;
        lastFace = Face(face);
    }
    mergedFaces.push_back(newFace);

    for (Face face : mergedFaces) {
        addTextureVertex(face, vertexData);
    }
}

void SubChunk::processSouthVertex(std::vector<Face> *faces, std::vector<int> *vertexData)
{
    if (faces[SOUTH].empty())
        return ;
    std::sort(faces[SOUTH].begin(), faces[SOUTH].end(), compareNorthFaces);
    std::vector<Face> mergedFacesZ;
    std::vector<Face> mergedFaces;

    bool isFirst = true;
    Face lastFace;
    Face newFace;
    for (Face face : faces[SOUTH])
    {
        if (isFirst || newFace.size.x > 31 || face.texture != newFace.texture || face.position.y != newFace.position.y || face.position.z != newFace.position.z || lastFace.position.x != face.position.x - _resolution)
        {
            if (!isFirst)
                mergedFacesZ.push_back(newFace);
            isFirst = false;
            newFace = Face(face);
        }
        newFace.size.x += _resolution;
        lastFace = Face(face);
    }
    mergedFacesZ.push_back(newFace);

    std::sort(mergedFacesZ.begin(), mergedFacesZ.end(), compareNorthStep2Faces);
    isFirst = true;
    for (Face face : mergedFacesZ)
    {
        if (isFirst || newFace.size.y > 31 || face.texture != newFace.texture || face.position.x != newFace.position.x || face.position.z != newFace.position.z || lastFace.position.y != face.position.y - _resolution || newFace.size.x != face.size.x)
        {
            if (!isFirst)
                mergedFaces.push_back(newFace);
            isFirst = false;
            newFace = Face(face);
        }
        newFace.size.y += _resolution;
        lastFace = Face(face);
    }
    mergedFaces.push_back(newFace);

    for (Face face : mergedFaces) {
        addTextureVertex(face, vertexData);
    }
}

void SubChunk::processEastVertex(std::vector<Face> *faces, std::vector<int> *vertexData)
{
    if (faces[EAST].empty())
        return ;
    std::sort(faces[EAST].begin(), faces[EAST].end(), compareEastFaces);
    std::vector<Face> mergedFacesZ;
    std::vector<Face> mergedFaces;

    bool isFirst = true;
    Face lastFace;
    Face newFace;
    for (Face face : faces[EAST])
    {
        if (isFirst || newFace.size.x > 31 || face.texture != newFace.texture || face.position.z != newFace.position.z || face.position.x != newFace.position.x || lastFace.position.y != face.position.y - _resolution)
        {
            if (!isFirst)
                mergedFacesZ.push_back(newFace);
            isFirst = false;
            newFace = Face(face);
        }
        newFace.size.x += _resolution;
        lastFace = Face(face);
    }
    mergedFacesZ.push_back(newFace);
    std::sort(mergedFacesZ.begin(), mergedFacesZ.end(), compareEastStep2Faces);
    isFirst = true;
    for (Face face : mergedFacesZ)
    {
        if (isFirst || newFace.size.y > 31 || face.texture != newFace.texture || face.position.x != newFace.position.x || face.position.y != newFace.position.y || lastFace.position.z != face.position.z - _resolution || newFace.size.x != face.size.x)
        {
            if (!isFirst)
                mergedFaces.push_back(newFace);
            isFirst = false;
            newFace = Face(face);
        }
        newFace.size.y += _resolution;
        lastFace = Face(face);
    }
    mergedFaces.push_back(newFace);

    for (Face face : mergedFaces) {
        addTextureVertex(face, vertexData);
    }
}

void SubChunk::processWestVertex(std::vector<Face> *faces, std::vector<int> *vertexData)
{
    if (faces[WEST].empty())
        return ;
    std::sort(faces[WEST].begin(), faces[WEST].end(), compareEastFaces);
    std::vector<Face> mergedFacesZ;
    std::vector<Face> mergedFaces;

    bool isFirst = true;
    Face lastFace;
    Face newFace;
    for (Face face : faces[WEST])
    {
        if (isFirst || newFace.size.y > 31 || face.texture != newFace.texture || face.position.z != newFace.position.z || face.position.x != newFace.position.x || lastFace.position.y != face.position.y - _resolution)
        {
            if (!isFirst)
                mergedFacesZ.push_back(newFace);
            isFirst = false;
            newFace = Face(face);
        }
        newFace.size.x += _resolution;
        lastFace = Face(face);
    }
    mergedFacesZ.push_back(newFace);
    std::sort(mergedFacesZ.begin(), mergedFacesZ.end(), compareEastStep2Faces);
    isFirst = true;
    for (Face face : mergedFacesZ)
    {
        if (isFirst || newFace.size.y > 31 || face.texture != newFace.texture || face.position.x != newFace.position.x || face.position.y != newFace.position.y || lastFace.position.z != face.position.z - _resolution || newFace.size.x != face.size.x)
        {
            if (!isFirst)
                mergedFaces.push_back(newFace);
            isFirst = false;
            newFace = Face(face);
        }
        newFace.size.y += _resolution;
        lastFace = Face(face);
    }
    mergedFaces.push_back(newFace);

    for (Face face : mergedFaces) {
        addTextureVertex(face, vertexData);
    }
}


void SubChunk::addFace(glm::ivec3 position, Direction dir, TextureType texture, bool isTransparent) {
    Face newFace;
    newFace.position = position;
    newFace.size = glm::vec2(0, 0);
    // newFace.size = glm::vec2(1, 1);
    newFace.direction = dir;
    newFace.texture = texture;
    if (isTransparent)
        _transparentFaces[dir].push_back(newFace);
    else
        _faces[dir].push_back(newFace);
}