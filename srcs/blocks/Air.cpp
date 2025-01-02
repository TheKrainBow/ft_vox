#include "blocks/Air.hpp"

Air::Air(int x, int y, int z) : ABlock(x, y, z)
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
