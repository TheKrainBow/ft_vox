#include "blocks/Dirt.hpp"

Dirt::Dirt(int x, int y, int z) : ABlock(x, y, z)
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
