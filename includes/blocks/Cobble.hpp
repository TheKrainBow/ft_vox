#pragma once

#include "ft_vox.hpp"
#include "ABlock.hpp"
#include "globals.hpp"

using namespace glm;

class Cobble : public ABlock
{
private:
	void initTexture();
public:
	Cobble(int x, int y, int z, int neighbors);
	Cobble(vec3 pos, int neighbors);
	~Cobble();
};

void Cobble::initTexture(void)
{
	_faceTextures[DOWN] = textManager.getTexture(COBBLE);
}

Cobble::Cobble(int x, int y, int z, int neighbors)
{
	_position = vec3(x, y, z);
	_neighbors = neighbors;
	initTexture();
}

Cobble::Cobble(vec3 pos, int neighbors)
{
	_position = pos;
	_neighbors = neighbors;
	initTexture();
}

Cobble::~Cobble()
{
}
