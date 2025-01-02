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
	Dirt(vec3 pos, int neighbors);
	~Dirt();
};

void Dirt::initTexture(void)
{
	_faceTextures[DOWN] = textManager.getTexture(DIRT);
}

Dirt::Dirt(int x, int y, int z, int neighbors)
{
	_position = vec3(x, y, z);
	_neighbors = neighbors;
	initTexture();
}

Dirt::Dirt(vec3 pos, int neighbors)
{
	_position = pos;
	_neighbors = neighbors;
	initTexture();
}

Dirt::~Dirt()
{
}
