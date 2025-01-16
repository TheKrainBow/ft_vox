
#include "Camera.hpp"


Camera::Camera() : position{0, -110, 0}, center{0.0f, 0.0f, 10.0f} { xangle = 0; yangle = 0;};
/*
	Moving the camera around (first person view)
*/
void Camera::move(float  forward, float  strafe, float up)
{
	// Calculate the direction based on the current angles
	float radiansX = xangle * (M_PI / 180.0);

	//float scaleForward = forward * cos(radiansY);

	// Determine the forward movement vector
	float forwardX = cos(radiansX) * forward;
	float forwardZ = sin(radiansX) * forward;

	// Determine the strafe movement vector (perpendicular to forward)
	float strafeX = cos(radiansX + M_PI / 2) * strafe;
	float strafeZ = sin(radiansX + M_PI / 2) * strafe;

	// Update theCcamera position
	position.z += (forwardX + strafeX) * movementspeed;
	position.x += (forwardZ + strafeZ) * movementspeed;
	center.x = position.x;
	center.z = position.z -10.0f;
	position.y += movementspeed * up;
	center.y = position.y;
}

/*
	Reset the camera position
*/
void Camera::reset()
{
	position = vec3(0.0, 0.0, 0.0);
	xangle = 0.0;
	yangle = 0.0;
	rotationspeed = 1;
	movementspeed = 0.1;
}

vec3 Camera::getWorldPosition(void)
{
	return (vec3(-position.x, -position.y, -position.z));
}

void Camera::updateMousePos(int x, int y)
{
	mousePos.x = static_cast<float>(x);
	mousePos.y = static_cast<float>(y);
}