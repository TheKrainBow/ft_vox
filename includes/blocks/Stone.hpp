#pragma once

#include "ft_vox.hpp"
#include "ABlock.hpp"
#include "globals.hpp"

using namespace glm;
class ABlock;

class Stone : public ABlock
{
private:
public:
	Stone(int x, int y, int z);
	~Stone();
};
