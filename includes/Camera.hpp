
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
		vec3 getWorldPosition();
		ivec2 getChunkPosition(int chunkSize);
		vec3 getPosition();
		vec2 getAngles();
		vec3 *getPositionPtr();
		vec2 *getAnglesPtr();
		e_direction *getDirectionPtr();
		void rotate(float xAngle, float yAngle, double rotationSpeed);

	private:
		vec3 position;
		vec2 angle;
		float rotationspeed = 125.0f;
		float movementspeed = 10.0f;
		bool mouseRotation = false;
		vec2 mousePos;
		std::mutex _positionMutex;
		e_direction _facing;
};
