#pragma once

#include <glm/glm.hpp>

class ABlock
{
	protected:
		enum e_direction {
			UP,
			DOWN,
			NORTH,
			SOUTH,
			WEST,
			EAST
		};
	protected:
		glm::vec3	_position;
		int			_neighbors;
		bool		_hasUpNeighbor;
		bool		_hasDownNeighbor;
		bool		_hasLeftNeighbor;
		bool		_hasRightNeighbor;
		bool		_hasFrontNeighbor;
		bool		_hasBackNeighbor;
		GLuint		_faceTextures[6];
		bool		_isAir = false;
	public:
		ABlock(int x, int y, int z, int neighbors);
		virtual ~ABlock() = default;
		virtual void display();
		vec3 getPosition(void);
	protected:
		virtual void displayNorthFace();
		virtual void displaySouthFace();
		virtual void displayEastFace();
		virtual void displayWestFace();
		virtual void displayUpFace();
		virtual void displayDownFace();
	
};

ABlock::ABlock(int x, int y, int z, int neighbors) {
	_hasUpNeighbor = (neighbors >> 0) & 1;
	_hasDownNeighbor = (neighbors >> 1) & 1;
	_hasLeftNeighbor = (neighbors >> 2) & 1;
	_hasRightNeighbor = (neighbors >> 3) & 1;
	_hasFrontNeighbor = (neighbors >> 4) & 1;
	_hasBackNeighbor = (neighbors >> 5) & 1;
	_position = vec3(x, y, z);
}

vec3 ABlock::getPosition(void)
{
	return (_position);
}

void ABlock::display() {
	if (_isAir)
		return ;
	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, _faceTextures[DOWN]);
	if (!_hasDownNeighbor)
		displayDownFace();
	if (!_hasDownNeighbor)
		displayUpFace();
	if (!_hasDownNeighbor)
		displayNorthFace();
	if (!_hasDownNeighbor)
		displaySouthFace();
	if (!_hasDownNeighbor)
		displayWestFace();
	if (!_hasDownNeighbor)
		displayEastFace();
	glDisable(GL_TEXTURE_2D);
}

void ABlock::displayDownFace() {
	glBegin(GL_QUADS);
	glTexCoord2f(0, 0);
	glVertex3d(_position.x, _position.y, _position.z);
	glTexCoord2f(1, 0);
	glVertex3d(_position.x + 1, _position.y, _position.z);
	glTexCoord2f(1, 1);
	glVertex3d(_position.x + 1, _position.y, _position.z + 1);
	glTexCoord2f(0, 1);
	glVertex3d(_position.x, _position.y, _position.z + 1);
	glEnd();
}

void ABlock::displayUpFace() {
	glBegin(GL_QUADS);
	glTexCoord2f(0, 0);
	glVertex3d(_position.x, _position.y + 1, _position.z);
	glTexCoord2f(1, 0);
	glVertex3d(_position.x + 1, _position.y + 1, _position.z);
	glTexCoord2f(1, 1);
	glVertex3d(_position.x + 1, _position.y + 1, _position.z + 1);
	glTexCoord2f(0, 1);
	glVertex3d(_position.x, _position.y + 1, _position.z + 1);
	glEnd();
}

void ABlock::displayNorthFace() {
	glBegin(GL_QUADS);
	glTexCoord2f(0, 0);
	glVertex3d(_position.x, _position.y, _position.z);
	glTexCoord2f(1, 0);
	glVertex3d(_position.x + 1, _position.y, _position.z);
	glTexCoord2f(1, 1);
	glVertex3d(_position.x + 1, _position.y + 1, _position.z);
	glTexCoord2f(0, 1);
	glVertex3d(_position.x, _position.y + 1, _position.z);
	glEnd();
}

void ABlock::displaySouthFace() {
	glBegin(GL_QUADS);
	glTexCoord2f(0, 0);
	glVertex3d(_position.x, _position.y, _position.z + 1);
	glTexCoord2f(1, 0);
	glVertex3d(_position.x + 1, _position.y, _position.z + 1);
	glTexCoord2f(1, 1);
	glVertex3d(_position.x + 1, _position.y + 1, _position.z + 1);
	glTexCoord2f(0, 1);
	glVertex3d(_position.x, _position.y + 1, _position.z + 1);
	glEnd();
}

void ABlock::displayWestFace() {
	glBegin(GL_QUADS);
	glTexCoord2f(0, 0);
	glVertex3d(_position.x + 1, _position.y, _position.z);
	glTexCoord2f(1, 0);
	glVertex3d(_position.x + 1, _position.y, _position.z + 1);
	glTexCoord2f(1, 1);
	glVertex3d(_position.x + 1, _position.y + 1, _position.z + 1);
	glTexCoord2f(0, 1);
	glVertex3d(_position.x + 1, _position.y + 1, _position.z);
	glEnd();
}

void ABlock::displayEastFace() {
	glBegin(GL_QUADS);
	glTexCoord2f(0, 0);
	glVertex3d(_position.x, _position.y, _position.z);
	glTexCoord2f(1, 0);
	glVertex3d(_position.x, _position.y, _position.z + 1);
	glTexCoord2f(1, 1);
	glVertex3d(_position.x, _position.y + 1, _position.z + 1);
	glTexCoord2f(0, 1);
	glVertex3d(_position.x, _position.y + 1, _position.z);
	glEnd();
}