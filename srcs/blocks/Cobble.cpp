#include "blocks/Cobble.hpp"

Cobble::Cobble(int x, int y, int z) : ABlock(x, y, z)
{
	_type = COBBLE;
	_textures[DOWN]  = T_COBBLE;
	_textures[UP]    = T_COBBLE;
	_textures[EAST]  = T_COBBLE;
	_textures[WEST]  = T_COBBLE;
	_textures[NORTH] = T_COBBLE;
	_textures[SOUTH] = T_COBBLE;
}


Cobble::~Cobble()
{
}
