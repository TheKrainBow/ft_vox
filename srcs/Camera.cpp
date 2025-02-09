#include "Camera.hpp"

Camera::Camera() : position{-226, -1500, 128}, center{0.0f, 0.0f, 10.0f} { angle.x = 0; angle.y = -90;};
/*
	Moving the camera around (first person view)
*/
void Camera::move(float  forward, float  strafe, float up)
{
	//std::lock_guard<std::mutex> lock(positionMutex);
	// Calculate the direction based on the current angles
	float radiansX = angle.x * (M_PI / 180.0);

	//float scaleForward = forward * cos(radiansY);

	// Determine the forward movement vector
	float forwardX = cos(radiansX) * forward;
	float forwardZ = sin(radiansX) * forward;

	// Determine the strafe movement vector (perpendicular to forward)
	float strafeX = cos(radiansX + M_PI / 2) * strafe;
	float strafeZ = sin(radiansX + M_PI / 2) * strafe;

	// Update theCcamera position
	_positionMutex.lock();
	position.z += (forwardX + strafeX) * movementspeed;
	position.x += (forwardZ + strafeZ) * movementspeed;
	center.x = position.x;
	center.z = position.z -10.0f;
	position.y += movementspeed * up;
	center.y = position.y;
	_positionMutex.unlock();
}

/*
	Reset the camera position
*/
void Camera::reset()
{
	_positionMutex.lock();
	position = vec3(0.0, 0.0, 0.0);
	_positionMutex.unlock();
	angle.x = 0.0;
	angle.y = 0.0;
	rotationspeed = 100.0f;
	movementspeed = 50.0f;
}

vec3 Camera::getWorldPosition(void)
{
	std::lock_guard<std::mutex> lock(_positionMutex);
	return (vec3(-position.x, -position.y, -position.z));
}

vec3 Camera::getPosition()
{
	std::lock_guard<std::mutex> lock(_positionMutex);
	return position;
}

vec3 Camera::getCenter()
{
	return center;
}

vec2 Camera::getAngles()
{
	return angle;
}

vec3 *Camera::getPositionPtr()
{
	std::lock_guard<std::mutex> lock(_positionMutex);
	return &position;
}

vec2 *Camera::getAnglesPtr()
{
	return &angle;
}

void Camera::rotate(float xAngle, float yAngle, double rotationSpeed)
{
	//std::lock_guard<std::mutex> lock(angleMutex);
	angle.x += xAngle * rotationSpeed;
	angle.y += yAngle * rotationSpeed;
	angle.y = clamp(angle.y, -90.0f, 90.0f);
	if (angle.x < 0)
		angle.x = 360;
	else if (angle.x > 360)
		angle.x = 0;
}

void Camera::updateMousePos(int x, int y)
{
	mousePos.x = static_cast<float>(x);
	mousePos.y = static_cast<float>(y);
}
