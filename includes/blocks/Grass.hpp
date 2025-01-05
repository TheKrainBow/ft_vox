#pragma once

#include "ft_vox.hpp"
#include "ABlock.hpp"
#include "globals.hpp"

using namespace glm;
class ABlock;

class Grass : public ABlock
{
private:
public:
	Grass(int x, int y, int z);
	virtual Grass *clone() const override;
	~Grass();
};
