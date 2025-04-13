
#pragma once

#include "ft_vox.hpp"

// Cardinal directions
enum e_direction
{
	N,
	NW,
	W,
	SW,
	S,
	SE,
	E,
	NE
};

class Camera {
	public:
		Camera();
		// Camera() : position{0.0, -30.0, 0.0}, center{0.0f, 0.0f, 10.0f} {};
		~Camera();
		void move(float forward, float strafe, float up);
		void reset();
		void updateMousePos(int x, int y);
		glm::vec3 getWorldPosition();
		glm::vec2 getChunkPosition(int chunkSize);
		glm::vec3 getPosition();
		glm::vec2 getAngles();
		glm::vec3 *getPositionPtr();
		glm::vec2 *getAnglesPtr();
		e_direction *getDirectionPtr();
		void rotate(float xAngle, float yAngle, double rotationSpeed);

	private:
		glm::vec3 position;
		glm::vec2 angle;
		float rotationspeed = 125.0f;
		float movementspeed = 10.0f;
		bool mouseRotation = false;
		glm::vec2 mousePos;
		std::mutex _positionMutex;
		e_direction _facing;
};
