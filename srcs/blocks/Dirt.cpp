#include "blocks/Dirt.hpp"

Dirt::Dirt(int x, int y, int z) : ABlock(x, y, z)
{
	_type = DIRT;
	_textures[DOWN]  = T_DIRT;
	_textures[UP]    = T_DIRT;
	_textures[EAST]  = T_DIRT;
	_textures[WEST]  = T_DIRT;
	_textures[NORTH] = T_DIRT;
	_textures[SOUTH] = T_DIRT;
}

Dirt *Dirt::clone() const
{
	return new Dirt(*this);
}

Dirt::~Dirt()
{
}
