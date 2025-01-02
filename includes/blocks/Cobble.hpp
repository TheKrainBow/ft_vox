#pragma once

#include "ft_vox.hpp"
#include "ABlock.hpp"
#include "globals.hpp"

using namespace glm;
class ABlock;

class Cobble : public ABlock
{
private:
	void initTexture();
public:
	Cobble(int x, int y, int z, int neighbors);
	virtual Cobble *clone() const override;
	~Cobble();
};
