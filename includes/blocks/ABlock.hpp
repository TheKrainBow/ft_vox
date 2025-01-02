#pragma once

#include <glm/glm.hpp>
#include "ft_vox.hpp"

class ABlock
{
	protected:
		enum e_direction {
			UP,
			DOWN,
			NORTH,
			SOUTH,
			WEST,
			EAST
		};
	protected:
		glm::vec3	_position;
		int			_neighbors;
		bool		_hasUpNeighbor;
		bool		_hasDownNeighbor;
		bool		_hasLeftNeighbor;
		bool		_hasRightNeighbor;
		bool		_hasFrontNeighbor;
		bool		_hasBackNeighbor;
		GLuint		_faceTextures[6];
		bool		_isAir = false;
	public:
		ABlock(int x, int y, int z, int neighbors);
		virtual ~ABlock() = default;
		virtual void display();
		vec3 getPosition(void);
	protected:
		virtual void displayNorthFace();
		virtual void displaySouthFace();
		virtual void displayEastFace();
		virtual void displayWestFace();
		virtual void displayUpFace();
		virtual void displayDownFace();
	
};
