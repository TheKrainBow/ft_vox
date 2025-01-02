#pragma once

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
		BlockType	_type;
	public:
		ABlock(int x, int y, int z);
		virtual ~ABlock() = default;
		virtual GLuint display(GLuint currentText);
		virtual ABlock* clone() const = 0;
		void updateNeighbors(int neighbors);
		vec3 getPosition(void);
		BlockType getType(void);
	protected:
		virtual void displayNorthFace();
		virtual void displaySouthFace();
		virtual void displayEastFace();
		virtual void displayWestFace();
		virtual void displayUpFace();
		virtual void displayDownFace();
	
};
