#include "blocks/Sand.hpp"

Sand::Sand(int x, int y, int z) : ABlock(x, y, z)
{
	_type = SAND;
	_textures[DOWN]  = T_SAND;
	_textures[UP]    = T_SAND;
	_textures[EAST]  = T_SAND;
	_textures[WEST]  = T_SAND;
	_textures[NORTH] = T_SAND;
	_textures[SOUTH] = T_SAND;
}

Sand::~Sand()
{
}
