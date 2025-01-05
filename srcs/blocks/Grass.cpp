#include "blocks/Grass.hpp"

Grass::Grass(int x, int y, int z) : ABlock(x, y, z)
{
	_type = GRASS;
	_textures[DOWN]  = T_DIRT;
	_textures[UP]    = T_GRASS_TOP;
	_textures[EAST]  = T_GRASS_SIDE;
	_textures[WEST]  = T_GRASS_SIDE;
	_textures[NORTH] = T_GRASS_SIDE;
	_textures[SOUTH] = T_GRASS_SIDE;
}

Grass::~Grass()
{
}
