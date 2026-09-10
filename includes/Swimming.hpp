#pragma once
#include <algorithm>
#include <cmath>

constexpr float SWIM_SURFACE_CLEARANCE = 0.12f;
constexpr float SWIM_EXIT_MARGIN = 0.25f;

constexpr float SWIM_BOB_AMPLITUDE = 0.09f;
constexpr float SWIM_BOB_PERIOD = 1.2f;

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
inline float advanceSwimming(float eyeY, float surfaceEyeY, bool rise, float dt, float& velocity) {
    float desired = rise ? std::clamp((surfaceEyeY - eyeY) * 6.0f, -2.5f, 2.5f) : -0.6f;
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
