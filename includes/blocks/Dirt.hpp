#pragma once

#include "ft_vox.hpp"
#include "ABlock.hpp"
#include "globals.hpp"

using namespace glm;

class Dirt : public ABlock
{
	private:
	public:
		Dirt(int x, int y, int z);
		~Dirt();
		Dirt *clone() const override;
};
