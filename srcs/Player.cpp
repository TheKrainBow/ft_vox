#include "Player.hpp"

Player::Player(Camera &cam, ChunkManager &chunkMgr) : _cam(cam), _chunkMgr(chunkMgr)
{
	initPlayerStates();
}

Player::~Player()
{
}

void Player::updateNow(std::chrono::steady_clock::time_point &now)
{
	_now = now;
}

void Player::updatePlayerStates()
{
	if (_gravity)
	{
		vec3 worldPos = _cam.getWorldPosition();
		BlockType camStandingBlock = AIR;
		BlockType camBodyBlockLegs = AIR;
		BlockType camBodyBlockTorso = AIR;
	
		// Compute world integer cell first
		int worldX = static_cast<int>(std::floor(worldPos.x));
		int worldZ = static_cast<int>(std::floor(worldPos.z));
	
		// Derive chunk strictly from the integer world cell to avoid float-boundary mismatches
		ivec2 chunkPos = {
			static_cast<int>(std::floor(static_cast<float>(worldX) / static_cast<float>(CHUNK_SIZE))),
			static_cast<int>(std::floor(static_cast<float>(worldZ) / static_cast<float>(CHUNK_SIZE)))};
	
		// Compute foot cell from eye height
		int footCell = static_cast<int>(std::floor(worldPos.y - EYE_HEIGHT + EPS));
		// int footSubY = static_cast<int>(std::floor(static_cast<float>(footCell) / static_cast<float>(CHUNK_SIZE)));
	
		// If the destination chunk or the needed subchunk is not yet loaded, avoid updating
		// ground/physics to prevent erroneous falling while streaming catches up.
		// if (Chunk* c = _chunkMgr.getChunk(chunkPos); c == nullptr || c->getSubChunk(std::max(0, footSubY)) == nullptr)
		// 	return;
	
		_camTopBlock = _chunkMgr.findBlockUnderPlayer(chunkPos, {worldX, static_cast<int>(std::floor(worldPos.y)), worldZ});
		camStandingBlock = _chunkMgr.getBlock(chunkPos, {worldX, footCell - 1, worldZ});
		camBodyBlockLegs = _chunkMgr.getBlock(chunkPos, {worldX, footCell, worldZ});
		camBodyBlockTorso = _chunkMgr.getBlock(chunkPos, {worldX, footCell + 1, worldZ});
	
		// Consider underwater slightly sooner by biasing eye sample downward
		const float eyeBias = 0.10f;
		int eyeCellY = static_cast<int>(std::floor(worldPos.y - eyeBias));
		BlockType camHeadBlock = _chunkMgr.getBlock(chunkPos, {worldX, eyeCellY, worldZ});
		_isUnderWater = (camHeadBlock == WATER);
	
		// Body blocks states debug
		// std::cout << '[' << camStandingBlock << ']' << std::endl;
		// std::cout << '[' << camBodyBlockLegs << ']' << std::endl;
		// std::cout << '[' << camBodyBlockTorso << ']' << std::endl;
	
		BlockType inWater = (camStandingBlock == WATER || camBodyBlockLegs == WATER || camBodyBlockTorso == WATER) ? WATER : AIR;
	
		BlockType camBodyOverHead = AIR;
		_ascending = _fallSpeed > 0.0;
		if (_ascending)
		{
			// Ceiling check
			camBodyOverHead = _chunkMgr.getBlock(chunkPos, {worldX, footCell + 2, worldZ});
			if (camBodyOverHead != WATER && camBodyOverHead != AIR)
			{
				_falling = false;
				_fallSpeed = 0.0;
			}
		}
		updateFalling(worldPos, _camTopBlock.height);
		updateSwimming(inWater);
		updateJumping();
	}
	else
	{
		vec3 worldPos = _cam.getWorldPosition();
		int worldX = static_cast<int>(std::floor(worldPos.x));
		int worldZ = static_cast<int>(std::floor(worldPos.z));
		ivec2 chunkPos = {
			static_cast<int>(std::floor(static_cast<float>(worldX) / static_cast<float>(CHUNK_SIZE))),
			static_cast<int>(std::floor(static_cast<float>(worldZ) / static_cast<float>(CHUNK_SIZE)))};
		const float eyeBias = 0.30f;
		int eyeCellY = static_cast<int>(std::floor(worldPos.y - eyeBias));
		BlockType camHeadBlock = _chunkMgr.getBlock(chunkPos, {worldX, eyeCellY, worldZ});
		_isUnderWater = (camHeadBlock == WATER);
	}
}

bool Player::canMove(const glm::vec3& offset, float extra)
{
	glm::vec3 probe = offset;
	if (glm::length(probe) > 0.0f)
		probe = glm::normalize(probe) * (glm::length(probe) + extra);

	vec3 nextCamPos = _cam.moveCheck(probe);
	float nextEyeY = -nextCamPos.y;
	int footCell = static_cast<int>(std::floor(nextEyeY - EYE_HEIGHT + EPS));
	int worldX = static_cast<int>(std::floor(-nextCamPos.x));
	int worldZ = static_cast<int>(std::floor(-nextCamPos.z));
	ivec3 worldPosFeet = {worldX, footCell, worldZ};
	ivec3 worldPosTorso = {worldX, footCell + 1, worldZ};

	// Derive chunk from integer world cell to avoid float-boundary errors
	ivec2 chunkPos = {
		static_cast<int>(std::floor(static_cast<float>(worldX) / static_cast<float>(CHUNK_SIZE))),
		static_cast<int>(std::floor(static_cast<float>(worldZ) / static_cast<float>(CHUNK_SIZE)))};

	// Check both the block in front of the legs and the upper body
	BlockType blockFeet = _chunkMgr.getBlock(chunkPos, worldPosFeet);
	BlockType blockTorso = _chunkMgr.getBlock(chunkPos, worldPosTorso);
	auto passable = [](BlockType b)
	{
		return (b == AIR || b == WATER || b == FLOWER_POPPY || b == FLOWER_DANDELION || b == FLOWER_CYAN || b == FLOWER_SHORT_GRASS || b == FLOWER_DEAD_BUSH);
	};
	return (passable(blockFeet) && passable(blockTorso));
}

// Divides every move into smaller steps if necessary typically if the player goes very
// fast to avoid block skips
bool Player::tryMoveStepwise(const glm::vec3 &moveVec, float stepSize)
{
	// Total distance to move
	float distance = glm::length(moveVec);
	if (distance <= 0.0f)
		return false;

	// Normalize and scale to step size
	glm::vec3 direction = glm::normalize(moveVec);
	float moved = 0.0f;

	while (moved < distance)
	{
		float step = std::min(stepSize, distance - moved);
		glm::vec3 offset = direction * step;

		if (!canMove(offset, 0.04f))
			return false;

		_cam.move(offset);
		moved += step;
	}
	return true;
}

void Player::initPlayerStates()
{
	// States
	_gravity			= GRAVITY;
	_falling			= FALLING;
	_swimming			= SWIMMING;
	_jumping			= JUMPING;
	_isUnderWater		= UNDERWATER;
	_ascending			= ASCENDING;
	_sprinting			= SPRINTING;
	_selectedBlock		= AIR;
	_placing			= KEY_INIT;

	// Cooldowns
	_now = std::chrono::steady_clock::now();
	_jumpCooldown = _now;
	_placeCooldown = _now;
	_swimUpCooldownOnRise = _now;
	_moveSpeed = 0.0;
	_rotationSpeed = 0.0;

	// Keys pressed
	bzero(keyStates, sizeof(keyStates));
}

void Player::findMoveRotationSpeed()
{
	// Calculate delta time
	static auto lastTime = std::chrono::steady_clock::now();
	auto currentTime = std::chrono::steady_clock::now();
	std::chrono::duration<float> elapsedTime = currentTime - lastTime;
	_deltaTime = std::min(elapsedTime.count(), 0.1f);
	lastTime = currentTime;

	// Apply delta to rotation and movespeed (legacy scheme)
	if (!_gravity && keyStates[GLFW_KEY_LEFT_CONTROL])
		_moveSpeed = (MOVEMENT_SPEED * ((20.0 * !_gravity) + (2 * _gravity))) * _deltaTime;
	else if (_gravity && _sprinting)
		_moveSpeed = (MOVEMENT_SPEED * ((20.0 * !_gravity) + (2 * _gravity))) * _deltaTime;
	else
		_moveSpeed = MOVEMENT_SPEED * _deltaTime;

	if (_swimming)
		_moveSpeed *= 0.5f;


	if (!isWSL())
		_rotationSpeed = (ROTATION_SPEED - 1.5) * _deltaTime;
	else
		_rotationSpeed = ROTATION_SPEED * _deltaTime;
}

// Player states updaters
void Player::updateFalling(vec3 &worldPos, int &blockHeight)
{
	// Target eye height above the ground block
	const float eyeTarget = blockHeight + 1 + EYE_HEIGHT;

	// Start falling if above ground
	if (!_falling && worldPos.y > eyeTarget + EPS)
		_falling = true;

	// Apply gravity only when enabled, not swimming, and currently falling
	if (_gravity && !_swimming && _falling)
	{
		// Update vertical velocity (per-second units)
		_fallSpeed -= GRAVITY_PER_SEC * _deltaTime;
		float decay = std::pow(FALL_DAMP_PER_TICK, TICK_RATE * _deltaTime);
		_fallSpeed *= decay;

		// Predict next camera world Y using the same scaling as Camera::move via moveCheck
		glm::vec3 offset(0.0f, -(_fallSpeed * _deltaTime), 0.0f);
		vec3 nextPosInternal = _cam.moveCheck(offset);
		float nextEyeY = -nextPosInternal.y;

		// If next step would cross the ground, snap this frame and stop falling
		if (nextEyeY <= eyeTarget + EPS)
		{
			_falling = false;
			_fallSpeed = 0.0f;
			_cam.setPos({-worldPos.x, -eyeTarget, -worldPos.z});
			return;
		}

		// Otherwise integrate normally this frame
		_cam.move(offset);
		return;
	}

	// If not falling (or gravity/swimming disabled), ensure we don't drift below ground
	if (_falling && worldPos.y <= eyeTarget + EPS)
	{
		_falling = false;
		_fallSpeed = 0.0f;
		_cam.setPos({-worldPos.x, -eyeTarget, -worldPos.z});
	}
}

void Player::updateSwimming(BlockType block)
{
	if (!_swimming && block == WATER)
	{
		_swimming = true;
	}
	if (_swimming && block != WATER)
	{
		_swimming = false;
		_fallSpeed = 0.0;
		_swimUpCooldownOnRise = std::chrono::steady_clock::now() + std::chrono::milliseconds(250);
	}
}

void Player::updateJumping()
{
	_jumping = _gravity && !_falling && keyStates[GLFW_KEY_SPACE] && _now > _jumpCooldown && !_swimming;
}


void Player::updatePlayerDirection()
{
	_playerDir = {0};

	// Build direction once
	if (keyStates[GLFW_KEY_W])
		_playerDir.forward += _moveSpeed;
	if (keyStates[GLFW_KEY_A])
		_playerDir.strafe += _moveSpeed;
	if (keyStates[GLFW_KEY_S])
		_playerDir.forward += -_moveSpeed;
	if (keyStates[GLFW_KEY_D])
		_playerDir.strafe += -_moveSpeed;
	if (!_gravity && keyStates[GLFW_KEY_SPACE])
		_playerDir.up += -_moveSpeed;
	if (!_gravity && keyStates[GLFW_KEY_LEFT_SHIFT])
		_playerDir.up += _moveSpeed;
	if (_jumping)
	{
		_fallSpeed = 1.2f;
		_playerDir.up += -_moveSpeed;
		_jumpCooldown = _now + std::chrono::milliseconds(400);
		_jumping = false;
	}
}

void Player::updateMovement()
{
	glm::vec3 moveVec = _cam.getForwardVector() * _playerDir.forward + _cam.getStrafeVector() * _playerDir.strafe + glm::vec3(0, 1, 0) * _playerDir.up;

	if (_gravity)
	{
		float stepSize = 0.5f;

		glm::vec3 moveX(moveVec.x, 0.0f, 0.0f);
		glm::vec3 moveZ(0.0f, 0.0f, moveVec.z);
		glm::vec3 moveY(0.0f, moveVec.y, 0.0f);

		if (moveX.x != 0.0f)
			tryMoveStepwise(moveX, stepSize);
		if (moveZ.z != 0.0f)
			tryMoveStepwise(moveZ, stepSize);
		if (moveY.y != 0.0f)
			tryMoveStepwise(moveY, stepSize);
	}
	else
		_cam.move(moveVec);
}

bool Player::updatePlacing()
{
	if (_placing && _now > _placeCooldown)
	{
		// Ray origin and direction in WORLD space
		glm::vec3 origin = _cam.getWorldPosition();
		glm::vec3 dir = _cam.getDirection();

		// Place block 5 block range
		bool placed = _chunkMgr.raycastPlaceOne(origin, dir, 5.0f, _selectedBlock);
		_placeCooldown = _now + std::chrono::milliseconds(150);
		if (placed)
		{
			return true;
		}
	}
	return false;
}

void Player::updateDeltaTime(float &newDelta)
{
	_deltaTime = newDelta;
}

void Player::updateSwimSpeed()
{
	// Water tweaks
	if (_swimming)
	{
		if (_gravity && _falling)
		{
			_fallSpeed = -0.25f;
		}
		if (keyStates[GLFW_KEY_SPACE] && std::chrono::steady_clock::now() > _swimUpCooldownOnRise)
		{
			_fallSpeed += 0.75f;
		}
	}
}

// Keys updater
void Player::keyAction(int key, int scancode, int action, int mods)
{
	(void)scancode;
	(void)mods;
	if (action == GLFW_PRESS)
		keyStates[key] = true;
	else if (action == GLFW_RELEASE)
		keyStates[key] = false;
}

// Game state setters
void Player::setPlaceCd()  { _placeCooldown = std::chrono::steady_clock::now() + std::chrono::milliseconds(150); }
void Player::setGravity(bool &gravity) { _gravity = gravity; }
void Player::setPlacing(bool placing) { _placing = placing; }
void Player::setSelectedBlock(BlockType block) { _selectedBlock = block; }

// Game state getters
bool Player::isSprinting() const { return _sprinting; }
bool Player::isUnderWater() const { return _isUnderWater; }
BlockType Player::getSelectedBlock() const { return _selectedBlock; }
bool Player::isPlacing() const { return _placing; }

// Toggles
void Player::toggleSprint() { _sprinting = !_sprinting; }
void Player::toggleGravity() {
	_gravity = !_gravity;
	
	if (_gravity)
	{
		// Smoothly re-enter gravity: reset vertical velocity to avoid snapping
		_falling = true;
		_fallSpeed = 0.0f;
	}
}
