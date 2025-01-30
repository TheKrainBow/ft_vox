#pragma once

#include "ft_vox.hpp"
#include "ABlock.hpp"
#include "globals.hpp"

using namespace glm;
class ABlock;

class Sand : public ABlock
{
private:
public:
	Sand(int x, int y, int z);
	~Sand();
};
