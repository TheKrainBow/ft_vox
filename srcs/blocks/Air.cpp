#include "blocks/Air.hpp"

Air::Air(int x, int y, int z, int neighbors) : ABlock(x, y, z, neighbors)
{
	_isAir = true;
}

Air::~Air()
{
}