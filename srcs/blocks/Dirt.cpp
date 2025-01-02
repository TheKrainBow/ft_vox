#include "blocks/Dirt.hpp"

Dirt::Dirt(int x, int y, int z, int neighbors) : ABlock(x, y, z, neighbors)
{
	_faceTextures[DOWN] = textManager.getTexture(DIRT);
	_type = DIRT;
}

Dirt::~Dirt()
{
}

Dirt *Dirt::clone() const
{
	return new Dirt(*this);
}
