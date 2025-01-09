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
		displayDownFace();
	if (_hasUpNeighbor)
		displayUpFace();
	if (_hasFrontNeighbor)
		displayNorthFace();
	if (_hasBackNeighbor)
		displaySouthFace();
	if (_hasLeftNeighbor)
		displayWestFace();
	if (_hasRightNeighbor)
		displayEastFace();
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

void ABlock::displayDownFace() {
	textManager.addTextureVertex(_textures[DOWN], DOWN, _position.x, _position.y, _position.z, 0, 0);
	textManager.addTextureVertex(_textures[DOWN], DOWN, _position.x + 1, _position.y, _position.z, 1, 0);
	textManager.addTextureVertex(_textures[DOWN], DOWN, _position.x + 1, _position.y, _position.z + 1, 1, 1);

	textManager.addTextureVertex(_textures[DOWN], DOWN, _position.x, _position.y, _position.z, 0, 0);
	textManager.addTextureVertex(_textures[DOWN], DOWN, _position.x + 1, _position.y, _position.z + 1, 1, 1);
	textManager.addTextureVertex(_textures[DOWN], DOWN, _position.x, _position.y, _position.z + 1, 0, 1);
}

void ABlock::displayUpFace() {
	textManager.addTextureVertex(_textures[UP], UP, _position.x, _position.y + 1, _position.z + 1, 0, 1);
	textManager.addTextureVertex(_textures[UP], UP, _position.x + 1, _position.y + 1, _position.z + 1, 1, 1);
	textManager.addTextureVertex(_textures[UP], UP, _position.x, _position.y + 1, _position.z, 0, 0);

	textManager.addTextureVertex(_textures[UP], UP, _position.x + 1, _position.y + 1, _position.z + 1, 1, 1);
	textManager.addTextureVertex(_textures[UP], UP, _position.x + 1, _position.y + 1, _position.z, 1, 0);
	textManager.addTextureVertex(_textures[UP], UP, _position.x, _position.y + 1, _position.z, 0, 0);
}

void ABlock::displayNorthFace() {
    textManager.addTextureVertex(_textures[NORTH], NORTH, _position.x + 1, _position.y + 1, _position.z, 1.0f, 1.0f);
    textManager.addTextureVertex(_textures[NORTH], NORTH, _position.x + 1, _position.y, _position.z, 1.0f, 0.0f);
    textManager.addTextureVertex(_textures[NORTH], NORTH, _position.x, _position.y, _position.z, 0.0f, 0.0f);

    textManager.addTextureVertex(_textures[NORTH], NORTH, _position.x, _position.y + 1, _position.z, 0.0f, 1.0f);
    textManager.addTextureVertex(_textures[NORTH], NORTH, _position.x + 1, _position.y + 1, _position.z, 1.0f, 1.0f);
    textManager.addTextureVertex(_textures[NORTH], NORTH, _position.x, _position.y, _position.z, 0.0f, 0.0f);
}

void ABlock::displaySouthFace() {
	textManager.addTextureVertex(_textures[SOUTH], SOUTH, _position.x, _position.y, _position.z + 1, 0.0f, 0.0f);
	textManager.addTextureVertex(_textures[SOUTH], SOUTH, _position.x + 1, _position.y, _position.z + 1, 1, 0);
	textManager.addTextureVertex(_textures[SOUTH], SOUTH, _position.x + 1, _position.y + 1, _position.z + 1, 1, 1);

	textManager.addTextureVertex(_textures[SOUTH], SOUTH, _position.x, _position.y, _position.z + 1, 0.0f, 0.0f);
	textManager.addTextureVertex(_textures[SOUTH], SOUTH, _position.x + 1, _position.y + 1, _position.z + 1, 1, 1);
	textManager.addTextureVertex(_textures[SOUTH], SOUTH, _position.x, _position.y + 1, _position.z + 1, 0, 1);
}

void ABlock::displayWestFace() {
	textManager.addTextureVertex(_textures[WEST], WEST, _position.x + 1, _position.y + 1, _position.z + 1, 1, 1);
	textManager.addTextureVertex(_textures[WEST], WEST, _position.x + 1, _position.y, _position.z + 1, 1, 0);
	textManager.addTextureVertex(_textures[WEST], WEST, _position.x + 1, _position.y, _position.z, 0, 0);

	textManager.addTextureVertex(_textures[WEST], WEST, _position.x + 1, _position.y + 1, _position.z, 0, 1);
	textManager.addTextureVertex(_textures[WEST], WEST, _position.x + 1, _position.y + 1, _position.z + 1, 1, 1);
	textManager.addTextureVertex(_textures[WEST], WEST, _position.x + 1, _position.y, _position.z, 0, 0);
}

void ABlock::displayEastFace() {
	textManager.addTextureVertex(_textures[EAST], EAST, _position.x, _position.y, _position.z, 0, 0);
	textManager.addTextureVertex(_textures[EAST], EAST, _position.x, _position.y, _position.z + 1, 1, 0);
	textManager.addTextureVertex(_textures[EAST], EAST, _position.x, _position.y + 1, _position.z + 1, 1, 1);

	textManager.addTextureVertex(_textures[EAST], EAST, _position.x, _position.y, _position.z, 0, 0);
	textManager.addTextureVertex(_textures[EAST], EAST, _position.x, _position.y + 1, _position.z + 1, 1, 1);
	textManager.addTextureVertex(_textures[EAST], EAST, _position.x, _position.y + 1, _position.z, 0, 1);
}
