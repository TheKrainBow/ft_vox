#include "blocks/ABlock.hpp"

ABlock::ABlock(int x, int y, int z) {
	_position = vec3(x, y, z);
	_type = AIR;
}


vec3 ABlock::getPosition(void)
{
	return (_position);
}

BlockType ABlock::getType(void)
{
	return (_type);
}

void ABlock::display() {
	if (_hasDownNeighbor)
		textManager.addTextureVertex(_textures[DOWN], DOWN, _position.x, _position.y, _position.z);
	if (_hasUpNeighbor)
		textManager.addTextureVertex(_textures[UP], UP, _position.x, _position.y, _position.z);
	if (_hasFrontNeighbor)
		textManager.addTextureVertex(_textures[NORTH], NORTH, _position.x, _position.y, _position.z);
	if (_hasBackNeighbor)
		textManager.addTextureVertex(_textures[SOUTH], SOUTH, _position.x, _position.y, _position.z);
	if (_hasLeftNeighbor)
		textManager.addTextureVertex(_textures[WEST], WEST, _position.x, _position.y, _position.z);
	if (_hasRightNeighbor)
		textManager.addTextureVertex(_textures[EAST], EAST, _position.x, _position.y, _position.z);
}

void ABlock::updateNeighbors(int neighbors)
{
	_hasUpNeighbor = (neighbors >> 0) & 1;
	_hasDownNeighbor = (neighbors >> 1) & 1;
	_hasLeftNeighbor = (neighbors >> 2) & 1;
	_hasRightNeighbor = (neighbors >> 3) & 1;
	_hasFrontNeighbor = (neighbors >> 4) & 1;
	_hasBackNeighbor = (neighbors >> 5) & 1;
}
