#pragma once

#include <glm/glm.hpp>

class ABlock
{
	protected:
		glm::vec3	_position;
		int			_neighbors;
		double		_r;
		double		_g;
		double		_b;
	public:
		virtual ~ABlock() = default;
		virtual void display() = 0;
		vec3 getPosition(void);
	protected:
		virtual void displayNorthFace() = 0;
		virtual void displaySouthFace() = 0;
		virtual void displayEastFace() = 0;
		virtual void displayWestFace() = 0;
		virtual void displayUpFace() = 0;
		virtual void displayDownFace() = 0;
	
};

vec3 ABlock::getPosition(void)
{
	return (_position);
}