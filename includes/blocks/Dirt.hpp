#pragma once

#include "ft_vox.hpp"
#include "ABlock.hpp"

using namespace glm;

class Dirt : public ABlock
{
private:
	void displayNorthFace() override;
	void displaySouthFace() override;
	void displayEastFace() override;
	void displayWestFace() override;
	void displayUpFace() override;
	void displayDownFace() override;
public:
	Dirt(int x, int y, int z, int neighbors);
	Dirt(vec3 pos, int neighbors);
	~Dirt();
	void display() override;
};

Dirt::Dirt(int x, int y, int z, int neighbors)
{
	_position = vec3(x, y, z);
	_neighbors = neighbors;
	_r = 1;
	_g = 0;
	_b = 0;
}

Dirt::Dirt(vec3 pos, int neighbors)
{
	_position = pos;
	_neighbors = neighbors;
	_r = 1.0;
	_g = 0;
	_b = 0;
}

Dirt::~Dirt()
{
}

void Dirt::display() {
	glColor3f(_r, _g, _b);
	displayDownFace();
	displayUpFace();
	displayNorthFace();
	displaySouthFace();
	displayWestFace();
	displayEastFace();
}

void Dirt::displayDownFace() {
	glBegin(GL_QUADS);
	glVertex3d(_position.x, _position.y, _position.z);
	glVertex3d(_position.x + 1, _position.y, _position.z);
	glVertex3d(_position.x + 1, _position.y, _position.z + 1);
	glVertex3d(_position.x, _position.y, _position.z + 1);
	glEnd();
}

void Dirt::displayUpFace() {
	glBegin(GL_QUADS);
	glVertex3d(_position.x, _position.y + 1, _position.z);
	glVertex3d(_position.x + 1, _position.y + 1, _position.z);
	glVertex3d(_position.x + 1, _position.y + 1, _position.z + 1);
	glVertex3d(_position.x, _position.y + 1, _position.z + 1);
	glEnd();
}

void Dirt::displayNorthFace() {
	glBegin(GL_QUADS);
	glVertex3d(_position.x, _position.y, _position.z);
	glVertex3d(_position.x + 1, _position.y, _position.z);
	glVertex3d(_position.x + 1, _position.y + 1, _position.z);
	glVertex3d(_position.x, _position.y + 1, _position.z);
	glEnd();
}

void Dirt::displaySouthFace() {
	glBegin(GL_QUADS);
	glVertex3d(_position.x, _position.y, _position.z + 1);
	glVertex3d(_position.x + 1, _position.y, _position.z + 1);
	glVertex3d(_position.x + 1, _position.y + 1, _position.z + 1);
	glVertex3d(_position.x, _position.y + 1, _position.z + 1);
	glEnd();
}

void Dirt::displayWestFace() {
	glBegin(GL_QUADS);
	glVertex3d(_position.x + 1, _position.y, _position.z);
	glVertex3d(_position.x + 1, _position.y, _position.z + 1);
	glVertex3d(_position.x + 1, _position.y + 1, _position.z + 1);
	glVertex3d(_position.x + 1, _position.y + 1, _position.z);
	glEnd();
}

void Dirt::displayEastFace() {
	glBegin(GL_QUADS);
	glVertex3d(_position.x, _position.y, _position.z);
	glVertex3d(_position.x, _position.y, _position.z + 1);
	glVertex3d(_position.x, _position.y + 1, _position.z + 1);
	glVertex3d(_position.x, _position.y + 1, _position.z);
	glEnd();
}