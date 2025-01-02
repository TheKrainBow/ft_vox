#include "blocks/Air.hpp"

Air::Air(int x, int y, int z, int neighbors) : ABlock(x, y, z, neighbors)
{
	_type = AIR;
}

Air::~Air()
{
}

Air *Air::clone() const
{
	return new Air(*this);
}
