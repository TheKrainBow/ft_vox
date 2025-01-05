#pragma once

#include "ft_vox.hpp"
#include "globals.hpp"

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
		TextureType	_textures[6];
		BlockType	_type;
		bool		_hasUpNeighbor;
		bool		_hasDownNeighbor;
		bool		_hasLeftNeighbor;
		bool		_hasRightNeighbor;
		bool		_hasFrontNeighbor;
		bool		_hasBackNeighbor;
	public:
		ABlock(int x, int y, int z);
		virtual ~ABlock() = default;
		virtual void display();
		virtual ABlock* clone() const = 0;
		vec3 getPosition(void);
		BlockType getType(void);
		void updateNeighbors(int neighbors);
	private:
		void displayNorthFace();
		void displaySouthFace();
		void displayEastFace();
		void displayWestFace();
		void displayUpFace();
		void displayDownFace();
};
