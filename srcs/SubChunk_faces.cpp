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


void SubChunk::processUpVertex()
{
    if (_faces[UP].empty())
        return ;
    std::sort(_faces[UP].begin(), _faces[UP].end(), compareUpFaces);
    std::vector<Face> mergedFacesZ;
    std::vector<Face> mergedFaces;

    bool isFirst = true;
    Face lastFace;
    Face newFace;
    for (Face face : _faces[UP])
    {
        if (isFirst || newFace.size.y > 31 || newFace.texture != face.texture || face.position.x != newFace.position.x || face.position.y != newFace.position.y || lastFace.position.z != face.position.z - 1)
        {
            if (isFirst == false)
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
        if (isFirst || newFace.size.x > 31 || face.texture != newFace.texture || face.position.y != newFace.position.y || face.position.z != newFace.position.z || lastFace.position.x != face.position.x - 1 || newFace.size.y != face.size.y)
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
        addTextureVertex(face);
    }
}

void SubChunk::processDownVertex()
{
    if (_faces[DOWN].empty())
        return ;
    std::sort(_faces[DOWN].begin(), _faces[DOWN].end(), compareUpFaces);
    std::vector<Face> mergedFacesZ;
    std::vector<Face> mergedFaces;

    bool isFirst = true;
    Face lastFace;
    Face newFace;
    for (Face face : _faces[DOWN])
    {
        if (isFirst || newFace.size.y > 31 || face.texture != newFace.texture || face.position.x != newFace.position.x || face.position.y != newFace.position.y || lastFace.position.z != face.position.z - 1)
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
        if (isFirst || newFace.size.x > 31 || face.texture != newFace.texture || face.position.y != newFace.position.y || face.position.z != newFace.position.z || lastFace.position.x != face.position.x - 1 || newFace.size.y != face.size.y)
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
        addTextureVertex(face);
    }
}

void SubChunk::processNorthVertex()
{
    if (_faces[NORTH].empty())
        return ;
    std::sort(_faces[NORTH].begin(), _faces[NORTH].end(), compareNorthFaces);
    std::vector<Face> mergedFacesZ;
    std::vector<Face> mergedFaces;

    bool isFirst = true;
    Face lastFace;
    Face newFace;
    for (Face face : _faces[NORTH])
    {
        if (isFirst || newFace.size.x > 31 || face.texture != newFace.texture || face.position.y != newFace.position.y || face.position.z != newFace.position.z || lastFace.position.x != face.position.x - 1)
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
        if (isFirst || newFace.size.y > 31 || face.texture != newFace.texture || face.position.x != newFace.position.x || face.position.z != newFace.position.z || lastFace.position.y != face.position.y - 1 || newFace.size.x != face.size.x)
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
        addTextureVertex(face);
    }
}

void SubChunk::processSouthVertex()
{
    if (_faces[SOUTH].empty())
        return ;
    std::sort(_faces[SOUTH].begin(), _faces[SOUTH].end(), compareNorthFaces);
    std::vector<Face> mergedFacesZ;
    std::vector<Face> mergedFaces;

    bool isFirst = true;
    Face lastFace;
    Face newFace;
    for (Face face : _faces[SOUTH])
    {
        if (isFirst || newFace.size.x > 31 || face.texture != newFace.texture || face.position.y != newFace.position.y || face.position.z != newFace.position.z || lastFace.position.x != face.position.x - 1)
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
        if (isFirst || newFace.size.y > 31 || face.texture != newFace.texture || face.position.x != newFace.position.x || face.position.z != newFace.position.z || lastFace.position.y != face.position.y - 1 || newFace.size.x != face.size.x)
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
        addTextureVertex(face);
    }
}

void SubChunk::processEastVertex()
{
    if (_faces[EAST].empty())
        return ;
    std::sort(_faces[EAST].begin(), _faces[EAST].end(), compareEastFaces);
    std::vector<Face> mergedFacesZ;
    std::vector<Face> mergedFaces;

    bool isFirst = true;
    Face lastFace;
    Face newFace;
    for (Face face : _faces[EAST])
    {
        if (isFirst || newFace.size.x > 31 || face.texture != newFace.texture || face.position.z != newFace.position.z || face.position.x != newFace.position.x || lastFace.position.y != face.position.y - 1)
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
        if (isFirst || newFace.size.y > 31 || face.texture != newFace.texture || face.position.x != newFace.position.x || face.position.y != newFace.position.y || lastFace.position.z != face.position.z - 1 || newFace.size.x != face.size.x)
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
        addTextureVertex(face);
    }
}

void SubChunk::processWestVertex()
{
    if (_faces[WEST].empty())
        return ;
    std::sort(_faces[WEST].begin(), _faces[WEST].end(), compareEastFaces);
    std::vector<Face> mergedFacesZ;
    std::vector<Face> mergedFaces;

    bool isFirst = true;
    Face lastFace;
    Face newFace;
    for (Face face : _faces[WEST])
    {
        if (isFirst || newFace.size.y > 31 || face.texture != newFace.texture || face.position.z != newFace.position.z || face.position.x != newFace.position.x || lastFace.position.y != face.position.y - 1)
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
        if (isFirst || newFace.size.y > 31 || face.texture != newFace.texture || face.position.x != newFace.position.x || face.position.y != newFace.position.y || lastFace.position.z != face.position.z - 1 || newFace.size.x != face.size.x)
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
        addTextureVertex(face);
    }
}


void SubChunk::addFace(vec3 position, Direction dir, TextureType texture) {
    Face newFace;
    newFace.position = position;
    newFace.size = vec2(0, 0);
    newFace.direction = dir;
    newFace.texture = texture;
    _faces[dir].push_back(newFace);
}