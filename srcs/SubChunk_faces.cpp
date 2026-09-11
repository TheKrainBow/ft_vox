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
		// Prevent merging of UP faces for logs and cactus (top caps never touch)
		if (isFirst || newFace.size.y > 31 || newFace.texture != face.texture || face.position.x != newFace.position.x || face.position.y != newFace.position.y || lastFace.position.z != face.position.z - _resolution || face.texture == T_LOG_TOP || face.texture == T_CACTUS_TOP)
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
		// Prevent merging of UP faces for logs and cactus (top caps never touch)
		if (isFirst || newFace.size.x > 31 || face.texture != newFace.texture || face.position.y != newFace.position.y || face.position.z != newFace.position.z || lastFace.position.x != face.position.x - _resolution || newFace.size.y != face.size.y || face.texture == T_LOG_TOP || face.texture == T_CACTUS_TOP)
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
		// Prevent merging of DOWN faces for logs and cactus (bottom caps never touch)
		if (isFirst || newFace.size.y > 31 || face.texture != newFace.texture || face.position.x != newFace.position.x || face.position.y != newFace.position.y || lastFace.position.z != face.position.z - _resolution || face.texture == T_LOG_TOP || face.texture == T_CACTUS_TOP)
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
		// Prevent merging of DOWN faces for logs and cactus (bottom caps never touch)
		if (isFirst || newFace.size.x > 31 || face.texture != newFace.texture || face.position.y != newFace.position.y || face.position.z != newFace.position.z || lastFace.position.x != face.position.x - _resolution || newFace.size.y != face.size.y || face.texture == T_LOG_TOP || face.texture == T_CACTUS_TOP)
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
		// Disable horizontal (X-axis) merging for log/cactus side faces to avoid elongated quads
		// when multiple blocks sit side-by-side. Vertical (Y) merging remains
		if (isFirst || newFace.size.x > 31 || face.texture != newFace.texture || face.position.y != newFace.position.y || face.position.z != newFace.position.z || lastFace.position.x != face.position.x - _resolution || face.texture == T_LOG_SIDE || face.texture == T_CACTUS_SIDE)
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
		// Disable horizontal (X-axis) merging for log/cactus side faces for SOUTH-facing quads
		if (isFirst || newFace.size.x > 31 || face.texture != newFace.texture || face.position.y != newFace.position.y || face.position.z != newFace.position.z || lastFace.position.x != face.position.x - _resolution || face.texture == T_LOG_SIDE || face.texture == T_CACTUS_SIDE)
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
		// Disable horizontal (Z-axis) merging for log/cactus side faces to avoid elongated quads
		if (isFirst || newFace.size.y > 31 || face.texture != newFace.texture || face.position.x != newFace.position.x || face.position.y != newFace.position.y || lastFace.position.z != face.position.z - _resolution || newFace.size.x != face.size.x || face.texture == T_LOG_SIDE || face.texture == T_CACTUS_SIDE)
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
		// Disable horizontal (Z-axis) merging for log/cactus side faces on WEST-facing quads
		if (isFirst || newFace.size.y > 31 || face.texture != newFace.texture || face.position.x != newFace.position.x || face.position.y != newFace.position.y || lastFace.position.z != face.position.z - _resolution || newFace.size.x != face.size.x || face.texture == T_LOG_SIDE || face.texture == T_CACTUS_SIDE)
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


void SubChunk::addFace(ivec3 position, Direction dir, TextureType texture, bool isTransparent) {
	Face newFace;
	newFace.position = position;
	newFace.size = ivec2(0, 0);
	// newFace.size = ivec2(1, 1);
	newFace.direction = dir;
	newFace.texture = texture;
	if (texture == T_WATER && _resolution == 1) {
		newFace.waterCorners = waterCorners(position);
		// Only the corners touching this face determine whether it is a full
		// rectangle. Bottom faces always lie on the block boundary.
		uint32_t corners = (newFace.waterCorners >> 15) & 0xffffu;
		uint32_t mask = 0xffffu;
		if (dir == NORTH) mask = 0x00ffu;
		if (dir == SOUTH) mask = 0xff00u;
		if (dir == WEST) mask = 0x0f0fu;
		if (dir == EAST) mask = 0xf0f0u;
		if (dir == DOWN || (corners & mask) == mask) {
			newFace.texture = T_WATER_FULL;
			newFace.waterCorners = 0;
		}
	}
	if (isTransparent)
		_transparentFaces[dir].push_back(newFace);
	else
		_faces[dir].push_back(newFace);
}

// Every cell sharing a corner samples the same four world cells, including
// across chunk boundaries. Flat source surfaces retain greedy meshing.
uint32_t SubChunk::waterCorners(ivec3 position) {
    auto read = [&](ivec3 local) -> char {
        if (local.x >= 0 && local.x < CHUNK_SIZE && local.y >= 0 &&
            local.y < CHUNK_SIZE && local.z >= 0 && local.z < CHUNK_SIZE)
            return getBlock(local);
        ivec3 world = _position * CHUNK_SIZE + local;
        return _chunkLoader.getBlock({(int)std::floor(double(world.x) / CHUNK_SIZE),
            (int)std::floor(double(world.z) / CHUNK_SIZE)}, world);
    };
    auto heights = waterSurfaceCorners([&](int x, int y, int z) {
        return read(position + ivec3(x, y, z));
    });
    uint32_t packed = 0;
    for (int i = 0; i < 4; ++i) packed |= uint32_t(heights[i]) << (4 * i);
    return packed == 0xeeeeu ? 0 : (0x80000000u | (packed << 15));
}
