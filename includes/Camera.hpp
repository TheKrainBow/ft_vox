
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

struct movedir
{
	float forward = 0.0f;
	float strafe = 0.0f;
	float up = 0.0f;
};

class Camera {
	public:
		Camera();
		// Camera() : position{0.0, -30.0, 0.0}, center{0.0f, 0.0f, 10.0f} {};
		~Camera();
		void move(const vec3 offset);
		vec3 moveCheck(const vec3 offset);
		void reset();
		void updateMousePos(int x, int y);
		vec3 getWorldPosition();
		ivec2 getChunkPosition(int chunkSize);
		vec3 getPosition();
		fvec2 getAngles();
		vec3 *getPositionPtr();
		fvec2 *getAnglesPtr();
		vec3 getDirection() const;
		e_direction *getDirectionPtr();
		float *getYPtr();
		void rotate(float xAngle, float yAngle, double rotationSpeed);
		void invert();
		void setPos(const float &x, const float &y, const float &z);
		void setPos(const vec3&newPos);
		vec3 getForwardVector() const;
		vec3 getStrafeVector() const;
	private:
		vec3 position;
		fvec2 angle;
		mutable std::mutex _angleMutex;
		float rotationspeed = 125.0f;
		float movementspeed = 10.0f;
		bool mouseRotation = false;
		ivec2 mousePos;
		std::mutex _positionMutex;
		e_direction _facing;
		float y_pos;
};
