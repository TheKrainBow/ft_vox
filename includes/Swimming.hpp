#pragma once
#include <algorithm>
#include <cmath>

constexpr float SWIM_SURFACE_CLEARANCE = 0.12f;
constexpr float SWIM_EXIT_MARGIN = 0.25f;

constexpr float SWIM_BOB_AMPLITUDE = 0.12f;
constexpr float SWIM_BOB_PERIOD = 0.9f;
constexpr float SWIM_HORIZONTAL_CURRENT_SPEED = 0.65f;
constexpr float SWIM_FALLING_CURRENT_SPEED = 1.5f;
constexpr float SWIM_UPSTREAM_RESISTANCE = 0.8f;

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

inline float advanceCurrent(float desired, float dt, float& velocity) {
    float decay = std::exp(-4.0f * dt);
    float displacement = desired * dt + (velocity - desired) * (1.0f - decay) / 4.0f;
    velocity = desired + (velocity - desired) * decay;
    return displacement;
}

inline float swimmingSurfaceTarget(float eyeY, float surfaceEyeY, bool rise, float dt, float& phase) {
    if (!rise || std::abs(eyeY - surfaceEyeY) >= 0.4f) {
        phase = 0.0f;
        return surfaceEyeY;
    }
    phase = std::fmod(phase + dt, SWIM_BOB_PERIOD);
    float blend = std::clamp(1.0f - std::abs(eyeY - surfaceEyeY) / 0.4f, 0.0f, 1.0f);
    blend = blend * blend * (3.0f - 2.0f * blend);
    return surfaceEyeY + SWIM_BOB_AMPLITUDE * blend *
        std::sin(6.28318530718f * phase / SWIM_BOB_PERIOD);
}

// World-space velocity and displacement; independent of camera movement scaling.
inline float advanceSwimming(float eyeY, float surfaceEyeY, bool rise, float dt, float& velocity,
                             float current = 0.0f) {
    float desired = rise ? std::clamp((surfaceEyeY - eyeY) * 6.0f, -2.5f, 2.5f) : -0.78f;
    desired += current;
    float decay = std::exp(-12.0f * dt);
    float displacement = desired * dt + (velocity - desired) * (1.0f - decay) / 12.0f;
    velocity = desired + (velocity - desired) * decay;
    float next = eyeY + displacement;
    if (rise && eyeY <= surfaceEyeY && next > surfaceEyeY) {
        next = surfaceEyeY;
        velocity = 0.0f;
    }
    return next;
}
