#include "blocks/Cobble.hpp"

Cobble::Cobble(int x, int y, int z) : ABlock(x, y, z)
{
	_faceTextures[DOWN] = textManager.getTexture(COBBLE);
	_type = COBBLE;
}

Cobble *Cobble::clone() const
{
	return new Cobble(*this);
}

Cobble::~Cobble()
{
}

void Cobble::initTexture() {}