#include "Camera.hpp"

Camera::Camera() : position{-908, -261, -1589}, angle(0, -90) {
	int test = int(angle.x) / 45;
	_facing = e_direction(test);
};

Camera::~Camera() {};

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
	position.y += movementspeed * up;
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

ivec2 Camera::getChunkPosition(int chunkSize) {
	ivec2 camChunk(-position.x / chunkSize, -position.z / chunkSize);
	if (-position.x < 0) camChunk.x--;
	if (-position.z < 0) camChunk.y--;
	return camChunk;
}

vec3 Camera::getPosition()
{
	std::lock_guard<std::mutex> lock(_positionMutex);
	return position;
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
	double tmp = angle.x + 45/2;
	if (tmp < 0)
		tmp = 360;
	else if (tmp > 360)
		tmp = 0;
	double test = int(tmp) / 45;
	_facing = e_direction((int)test);
}

e_direction *Camera::getDirectionPtr()
{
	return &_facing;
}

void Camera::updateMousePos(int x, int y)
{
	mousePos.x = static_cast<float>(x);
	mousePos.y = static_cast<float>(y);
}
