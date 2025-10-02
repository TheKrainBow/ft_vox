#pragma once

#include "ft_vox.hpp"
#include "Camera.hpp"
#include "define.hpp"
#include "ChunkManager.hpp"

class Player
{
private:
	// References
	Camera			&_cam;
	ChunkManager	&_chunkMgr;

	// Player states
	bool _falling;
	bool _swimming;
	bool _jumping;
	bool _isUnderWater;
	bool _ascending;
	bool _sprinting;
	bool _gravity;

	// Player speed
	float _moveSpeed;
	float _rotationSpeed;
	float _fallSpeed = 0.0f;
	float _deltaTime;

	// Selected block for placement
	BlockType	_selectedBlock;
	bool _placing;

	// Player actions cooldown
	std::chrono::steady_clock::time_point _jumpCooldown;
	std::chrono::steady_clock::time_point _swimUpCooldownOnRise;
	std::chrono::steady_clock::time_point _placeCooldown;
	std::chrono::steady_clock::time_point _now;

	// Key states of the player
	bool keyStates[348];

	// Movement info
	movedir _playerDir;

	// Game info
	TopBlock _camTopBlock;
public:
	Player(Camera &cam, ChunkManager &chunkMgr);
	~Player();
	
	// Player states update
	void updatePlayerStates();
	void updateNow(std::chrono::steady_clock::time_point &now);
	void setGravity(bool &gravity);
	void updateDeltaTime(float &newDelta);
	void keyAction(int key, int scancode, int action, int mods);
	bool tryMoveStepwise(const glm::vec3 &moveVec, float stepSize);
    bool isSprinting() const;
    bool isUnderWater() const;
    bool isSwimming() const { return _swimming; }
	void updateMovement();
	void updateSwimSpeed();
	void findMoveRotationSpeed();
	bool updatePlacing();
	void updatePlayerDirection();

	BlockType getSelectedBlock() const;
	void setSelectedBlock(BlockType block);
	bool isPlacing() const;
	void setPlaceCd();
	void setPlacing(bool placing);
	void toggleSprint();
	void toggleGravity();
private:
	// Movement check
	bool canMove(const glm::vec3& offset, float extra);

	// Udate player states
	void updateFalling(vec3 &worldPos, int &blockHeight);
	void updateSwimming(BlockType block);
	void updateJumping();

	// Init
	void initPlayerStates();
};
