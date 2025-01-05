#include "blocks/Stone.hpp"

Stone::Stone(int x, int y, int z) : ABlock(x, y, z)
{
	_type = STONE;
	_textures[DOWN]  = T_STONE;
	_textures[UP]    = T_STONE;
	_textures[EAST]  = T_STONE;
	_textures[WEST]  = T_STONE;
	_textures[NORTH] = T_STONE;
	_textures[SOUTH] = T_STONE;
}

Stone *Stone::clone() const
{
	return new Stone(*this);
}

Stone::~Stone()
{
}
