
#pragma once

#include "ft_vox.hpp"

class Camera {
	public:
		Camera();
		// Camera() : position{0.0, -30.0, 0.0}, center{0.0f, 0.0f, 10.0f} {};
		~Camera() {};
		void move(float forward, float strafe, float up);
		void reset();
		void updateMousePos(int x, int y);
		vec3 getWorldPosition(void);

		vec3 position;
		vec3 center;
		float xangle = 0.0f;
		float yangle = 0.0f;
		float rotationspeed = 125.0f;
		float movementspeed = 50.0f;
		bool mouseRotation = false;
		vec2 mousePos;
};
