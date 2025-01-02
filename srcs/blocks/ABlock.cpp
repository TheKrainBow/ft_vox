#include "blocks/ABlock.hpp"

ABlock::ABlock(int x, int y, int z) {
	_position = vec3(x, y, z);
	_type = AIR;
	updateNeighbors(0);
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

vec3 ABlock::getPosition(void)
{
	return (_position);
}

BlockType ABlock::getType(void)
{
	return (_type);
}

GLuint ABlock::display(GLuint currentText) {
	if (currentText != _faceTextures[DOWN])
		glBindTexture(GL_TEXTURE_2D, _faceTextures[DOWN]);
	glBegin(GL_QUADS);
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
	glEnd();
	return (_faceTextures[DOWN]);
}

void ABlock::displayDownFace() {
	glTexCoord2f(0, 0);
	glVertex3d(_position.x, _position.y, _position.z);
	glTexCoord2f(1, 0);
	glVertex3d(_position.x + 1, _position.y, _position.z);
	glTexCoord2f(1, 1);
	glVertex3d(_position.x + 1, _position.y, _position.z + 1);
	glTexCoord2f(0, 1);
	glVertex3d(_position.x, _position.y, _position.z + 1);
}

void ABlock::displayUpFace() {
	glTexCoord2f(0, 0);
	glVertex3d(_position.x, _position.y + 1, _position.z);
	glTexCoord2f(1, 0);
	glVertex3d(_position.x + 1, _position.y + 1, _position.z);
	glTexCoord2f(1, 1);
	glVertex3d(_position.x + 1, _position.y + 1, _position.z + 1);
	glTexCoord2f(0, 1);
	glVertex3d(_position.x, _position.y + 1, _position.z + 1);
}

void ABlock::displayNorthFace() {
	glTexCoord2f(0, 0);
	glVertex3d(_position.x, _position.y, _position.z);
	glTexCoord2f(1, 0);
	glVertex3d(_position.x + 1, _position.y, _position.z);
	glTexCoord2f(1, 1);
	glVertex3d(_position.x + 1, _position.y + 1, _position.z);
	glTexCoord2f(0, 1);
	glVertex3d(_position.x, _position.y + 1, _position.z);
}

void ABlock::displaySouthFace() {
	glTexCoord2f(0, 0);
	glVertex3d(_position.x, _position.y, _position.z + 1);
	glTexCoord2f(1, 0);
	glVertex3d(_position.x + 1, _position.y, _position.z + 1);
	glTexCoord2f(1, 1);
	glVertex3d(_position.x + 1, _position.y + 1, _position.z + 1);
	glTexCoord2f(0, 1);
	glVertex3d(_position.x, _position.y + 1, _position.z + 1);
}

void ABlock::displayWestFace() {
	glTexCoord2f(0, 0);
	glVertex3d(_position.x + 1, _position.y, _position.z);
	glTexCoord2f(1, 0);
	glVertex3d(_position.x + 1, _position.y, _position.z + 1);
	glTexCoord2f(1, 1);
	glVertex3d(_position.x + 1, _position.y + 1, _position.z + 1);
	glTexCoord2f(0, 1);
	glVertex3d(_position.x + 1, _position.y + 1, _position.z);
}

void ABlock::displayEastFace() {
	glTexCoord2f(0, 0);
	glVertex3d(_position.x, _position.y, _position.z);
	glTexCoord2f(1, 0);
	glVertex3d(_position.x, _position.y, _position.z + 1);
	glTexCoord2f(1, 1);
	glVertex3d(_position.x, _position.y + 1, _position.z + 1);
	glTexCoord2f(0, 1);
	glVertex3d(_position.x, _position.y + 1, _position.z);
}
