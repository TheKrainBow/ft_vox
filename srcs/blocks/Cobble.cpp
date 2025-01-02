#include "blocks/Cobble.hpp"

Cobble::Cobble(int x, int y, int z, int neighbors) : ABlock(x, y, z, neighbors)
{
	_faceTextures[DOWN] = textManager.getTexture(COBBLE);
	_type = COBBLE;
}

Cobble::~Cobble()
{
}

void Cobble::initTexture() {}