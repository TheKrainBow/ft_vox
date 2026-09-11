#pragma once
#include "Water.hpp"
#include <algorithm>
#include <cmath>

constexpr float SWIM_HORIZONTAL_CURRENT_SPEED = 0.45f;
constexpr float SWIM_FALLING_CURRENT_SPEED = 1.5f;
constexpr float SWIM_UPSTREAM_RESISTANCE = 0.25f;

// Preserve the old source-water exit height, shifted with the visible waterline.
inline bool swimmingSurfaceContact(float feetY, float surfaceY) {
    constexpr float clearance = (2.0f - waterHeight(WATER) / 15.0f) * 0.8f;
    return feetY + EPS < surfaceY + clearance;
}

// Reduce only the input component opposing the current, in world space.
inline void resistSwimmingCurrent(float& x, float& z, float currentX, float currentZ) {
    float strength = std::sqrt(currentX * currentX + currentZ * currentZ);
    if (strength == 0.0f) return;
    float dx = currentX / strength, dz = currentZ / strength;
    float upstream = std::min(0.0f, x * dx + z * dz);
    float resistance = SWIM_UPSTREAM_RESISTANCE * std::min(strength, 1.0f);
    x -= dx * upstream * resistance;
    z -= dz * upstream * resistance;
}

inline float advanceCurrent(float desired, float dt, float& velocity, float response = 4.0f) {
    float decay = std::exp(-response * dt);
    float displacement = desired * dt + (velocity - desired) * (1.0f - decay) / response;
    velocity = desired + (velocity - desired) * decay;
    return displacement;
}

// Original 20 Hz swimming: sink between upward pulses while Space is held.
inline float swimmingTickVelocity(float velocity, bool falling, bool rise) {
    if (falling) velocity = -0.25f;
    if (rise) velocity += 0.75f;
    return velocity;
}
