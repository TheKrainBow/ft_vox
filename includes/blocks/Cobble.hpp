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
	~Cobble();
};

Cobble::Cobble(int x, int y, int z, int neighbors) : ABlock(x, y, z, neighbors)
{
	_faceTextures[DOWN] = textManager.getTexture(COBBLE);
}

Cobble::~Cobble()
{
}
