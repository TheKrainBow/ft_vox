#pragma once

#include "ft_vox.hpp"
#include "ABlock.hpp"
#include "globals.hpp"

using namespace glm;

class Air : public ABlock
{
private:
public:
	Air(int x, int y, int z, int neighbors);
	~Air();
};

Air::Air(int x, int y, int z, int neighbors) : ABlock(x, y, z, neighbors)
{
	_isAir = true;
}

Air::~Air()
{
}
