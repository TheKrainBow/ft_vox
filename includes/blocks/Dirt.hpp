#pragma once

#include "ft_vox.hpp"
#include "ABlock.hpp"
#include "globals.hpp"

using namespace glm;

class Dirt : public ABlock
{
private:
	void initTexture();
public:
	Dirt(int x, int y, int z, int neighbors);
	~Dirt();
};

Dirt::Dirt(int x, int y, int z, int neighbors) : ABlock(x, y, z, neighbors)
{
	_faceTextures[DOWN] = textManager.getTexture(DIRT);
}

Dirt::~Dirt()
{
}
