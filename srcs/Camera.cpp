#include "Camera.hpp"

Camera::Camera() : position{-819, -143, -1359}, angle(270, -42) {
	_facing = e_direction(int(angle.x) / 45);
};

Camera::~Camera() {};

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

fvec2 Camera::getAngles()
{
	return angle;
}

vec3 *Camera::getPositionPtr()
{
	std::lock_guard<std::mutex> lock(_positionMutex);
	return &position;
}

fvec2 *Camera::getAnglesPtr()
{
	return &angle;
}

vec3 Camera::getDirection() const {
	float pitch = radians(angle.y); // vertical
	float yaw   = radians(angle.x); // horizontal

	vec3 dir;
	dir.z = -cos(pitch) * cos(yaw);
	dir.y = sin(pitch);
	dir.x = -cos(pitch) * sin(yaw);
	return normalize(dir);
}

void Camera::rotate(float xAngle, float yAngle, double rotationSpeed)
{
	//std::lock_guard<std::mutex> lock(angleMutex);
	angle.x += xAngle * rotationSpeed;
	angle.y += yAngle * rotationSpeed;
	angle.y = std::clamp(angle.y, -90.0f, 90.0f);
	if (angle.x < 0)
		angle.x += 360;
	else if (angle.x > 360)
		angle.x -= 360;
	_facing = e_direction((((int)angle.x + 45/2) % 360) / 45);
}

void Camera::invert()
{
	angle.x += 180.0f;
	if (angle.x >= 360.0f)
		angle.x -= 360.0f;
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

void Camera::setPos(const float &x, const float &y, const float &z)
{
	position = {x, y, z};
}

void Camera::setPos(const vec3 &newPos)
{
	position = newPos;

}

vec3 Camera::getForwardVector() const
{
	float yawRad = angle.x * (M_PI / 180.0f);
	return vec3(
		sin(yawRad),	// X axis
		
		0.0f,
		cos(yawRad)		// Z axis
	);
}

vec3 Camera::getStrafeVector() const
{
	float yawRad = (angle.x + 90.0f) * (M_PI / 180.0f);
	return vec3(
		sin(yawRad),
		0.0f,
		cos(yawRad)
	);
}

/*
	Moving the camera around (first person view)
*/
void Camera::move(const vec3 offset)
{
	std::lock_guard<std::mutex> lock(_positionMutex);
	position.x += offset.x * movementspeed;
	position.y += offset.y * movementspeed;
	position.z += offset.z * movementspeed;
}

/*
	Send the potential position if we move
*/
vec3 Camera::moveCheck(const vec3 offset)
{
	// Copy current position
	_positionMutex.lock();
	vec3 nextPos = position;
	_positionMutex.unlock();

	// Apply world-space offset
	nextPos.x += offset.x * movementspeed;
	nextPos.y += offset.y * movementspeed;
	nextPos.z += offset.z * movementspeed;

	return nextPos;
}
