#pragma once

#include "ft_vox.hpp"
#include "ABlock.hpp"

using namespace glm;

class Cobble : public ABlock
{
private:
	void displayNorthFace() override;
	void displaySouthFace() override;
	void displayEastFace() override;
	void displayWestFace() override;
	void displayUpFace() override;
	void displayDownFace() override;
public:
	Cobble(int x, int y, int z, int neighbors);
	Cobble(vec3 pos, int neighbors);
	~Cobble();
	void display() override;
};

Cobble::Cobble(int x, int y, int z, int neighbors)
{
	_position = vec3(x, y, z);
	_neighbors = neighbors;
	_r = 0.5;
	_g = 0.5;
	_b = 0.5;
}

Cobble::Cobble(vec3 pos, int neighbors)
{
	_position = pos;
	_neighbors = neighbors;
	_r = 0.5;
	_g = 0.5;
	_b = 0.5;
}

Cobble::~Cobble()
{
}

void Cobble::display() {
	glColor3f(_r, _g, _b);
	displayDownFace();
	displayUpFace();
	displayNorthFace();
	displaySouthFace();
	displayWestFace();
	displayEastFace();
}

void Cobble::displayDownFace() {
	glBegin(GL_QUADS);
	glVertex3d(_position.x, _position.y, _position.z);
	glVertex3d(_position.x + 1, _position.y, _position.z);
	glVertex3d(_position.x + 1, _position.y, _position.z + 1);
	glVertex3d(_position.x, _position.y, _position.z + 1);
	glEnd();
}

void Cobble::displayUpFace() {
	glBegin(GL_QUADS);
	glVertex3d(_position.x, _position.y + 1, _position.z);
	glVertex3d(_position.x + 1, _position.y + 1, _position.z);
	glVertex3d(_position.x + 1, _position.y + 1, _position.z + 1);
	glVertex3d(_position.x, _position.y + 1, _position.z + 1);
	glEnd();
}

void Cobble::displayNorthFace() {
	glBegin(GL_QUADS);
	glVertex3d(_position.x, _position.y, _position.z);
	glVertex3d(_position.x + 1, _position.y, _position.z);
	glVertex3d(_position.x + 1, _position.y + 1, _position.z);
	glVertex3d(_position.x, _position.y + 1, _position.z);
	glEnd();
}

void Cobble::displaySouthFace() {
	glBegin(GL_QUADS);
	glVertex3d(_position.x, _position.y, _position.z + 1);
	glVertex3d(_position.x + 1, _position.y, _position.z + 1);
	glVertex3d(_position.x + 1, _position.y + 1, _position.z + 1);
	glVertex3d(_position.x, _position.y + 1, _position.z + 1);
	glEnd();
}

void Cobble::displayWestFace() {
	glBegin(GL_QUADS);
	glVertex3d(_position.x + 1, _position.y, _position.z);
	glVertex3d(_position.x + 1, _position.y, _position.z + 1);
	glVertex3d(_position.x + 1, _position.y + 1, _position.z + 1);
	glVertex3d(_position.x + 1, _position.y + 1, _position.z);
	glEnd();
}

void Cobble::displayEastFace() {
	glBegin(GL_QUADS);
	glVertex3d(_position.x, _position.y, _position.z);
	glVertex3d(_position.x, _position.y, _position.z + 1);
	glVertex3d(_position.x, _position.y + 1, _position.z + 1);
	glVertex3d(_position.x, _position.y + 1, _position.z);
	glEnd();
}