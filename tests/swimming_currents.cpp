#include "Swimming.hpp"
#include "Water.hpp"
#include <cassert>
#include <iostream>

int main() {
    // Each flow level lowers the surface pulse's exit point; sources retain
    // the original two-block feet height. Check both positive and negative Y.
    for (float base : {-10.0f, 10.0f}) {
        float previousExit = base + 2.0f;
        for (char block : std::array<char, 7>{WATER, WATER_FLOW_1, WATER_FLOW_2,
                WATER_FLOW_3, WATER_FLOW_4, WATER_FLOW_5, WATER_FLOW_6}) {
            auto corners = waterSurfaceCorners([&](int, int y, int) -> char {
                return y == 0 ? block : AIR;
            });
            float surface = base + waterSurfaceHeight(corners, 0.5f, 0.5f);
            float exit = base + 2.0f - EPS + (waterHeight(block) - waterHeight(WATER)) / 15.0f;
            assert(swimmingSurfaceContact(exit - 0.01f, surface));
            assert(!swimmingSurfaceContact(exit + 0.01f, surface));
            if (block != WATER) assert(exit < previousExit);
            previousExit = exit;
        }
        // Full waterfalls and covered stream cells use their actual full height.
        for (char block : {WATER_FALLING, WATER_FLOW_6}) {
            auto corners = waterSurfaceCorners([&](int, int y, int) -> char {
                return y == 0 ? block : (y == 1 ? WATER_FALLING : AIR);
            });
            float surface = base + waterSurfaceHeight(corners, 0.5f, 0.5f);
            assert(swimmingSurfaceContact(base + 2.0f, surface));
            assert(!swimmingSurfaceContact(base + 2.1f, surface));
        }
    }
    auto stream = [](int x, int y, int z) -> char {
        if (y || z) return STONE;
        if (x == -1) return WATER;
        if (x == 0) return WATER_FLOW_1;
        if (x == 1) return WATER_FLOW_2;
        return AIR;
    };
    auto flow = waterCurrent(stream);
    assert(flow[0] == 1.0f && flow[1] == 0.0f && flow[2] == 0.0f);
    auto reversed = waterCurrent([&](int x, int y, int z) { return stream(-x, y, z); });
    assert(reversed[0] == -1.0f);
    auto rotated = waterCurrent([&](int x, int y, int z) { return stream(z, y, x); });
    assert(rotated[2] == 1.0f);
    // The first stream cell pushes even before another stream cell exists,
    // and when it leads immediately into a waterfall.
    for (char outlet : std::array<char, 2>{AIR, WATER_FALLING}) {
        auto first = waterCurrent([&](int x, int y, int z) -> char {
            if (y || z) return STONE;
            if (x == -1) return WATER;
            if (x == 0) return WATER_FLOW_1;
            return outlet;
        });
        assert(first[0] == 1.0f && first[2] == 0.0f);
    }
    float upstreamX = -10.0f, sidewaysZ = 3.0f;
    resistSwimmingCurrent(upstreamX, sidewaysZ, 1.0f, 0.0f);
    assert(std::abs(upstreamX + 7.5f) < 0.0001f && sidewaysZ == 3.0f);
    float downstreamX = 10.0f, idleZ = 0.0f;
    resistSwimmingCurrent(downstreamX, idleZ, 1.0f, 0.0f);
    assert(downstreamX == 10.0f && idleZ == 0.0f);
    float idleX = 0.0f;
    resistSwimmingCurrent(idleX, idleZ, 1.0f, 0.0f);
    assert(idleX == 0.0f && idleZ == 0.0f);
    for (char block : std::array<char, 4>{AIR, WATER, WATER_FLOW_3, WATER_FALLING}) {
        auto uniform = waterCurrent([&](int, int, int) { return block; });
        assert(uniform[0] == 0.0f && uniform[2] == 0.0f);
        assert(uniform[1] == (block == WATER_FALLING ? -1.0f : 0.0f));
    }
    auto diagonal = waterCurrent([](int x, int, int z) {
        return (x == -1 || z == -1) ? WATER : WATER_FLOW_1;
    });
    assert(std::abs(diagonal[0] - std::sqrt(0.5f)) < 0.0001f);
    assert(std::abs(diagonal[2] - diagonal[0]) < 0.0001f);

    // Idle drift ramps up gently and integrates equally at different frame rates.
    float coarseVelocity = 0.0f, fineVelocity = 0.0f, fineDistance = 0.0f;
    float coarseDistance = advanceCurrent(SWIM_HORIZONTAL_CURRENT_SPEED, 1.0f, coarseVelocity);
    for (int i = 0; i < 100; ++i)
        fineDistance += advanceCurrent(SWIM_HORIZONTAL_CURRENT_SPEED, 0.01f, fineVelocity);
    assert(coarseDistance > 0.0f && coarseDistance < SWIM_HORIZONTAL_CURRENT_SPEED);
    assert(std::abs(coarseDistance - fineDistance) < 0.0001f);
    assert(std::abs(coarseVelocity - fineVelocity) < 0.0001f);
    // The same downstream displacement reduces upstream progress.
    assert(-1.0f + coarseDistance < 0.0f && -1.0f + coarseDistance > -1.0f);

    // Even modest upstream input overcomes a fully established current.
    // Exercise resistance and drift together, including a diagonal stream.
    for (float dt : {0.01f, 0.05f, 0.1f}) {
        for (float angle : {0.0f, 0.78539816339f}) {
            float dx = std::cos(angle), dz = std::sin(angle);
            float x = -dx * dt, z = -dz * dt;
            resistSwimmingCurrent(x, z, dx, dz);
            float vx = dx * SWIM_HORIZONTAL_CURRENT_SPEED;
            float vz = dz * SWIM_HORIZONTAL_CURRENT_SPEED;
            x += advanceCurrent(vx, dt, vx);
            z += advanceCurrent(vz, dt, vz);
            float upstreamProgress = -(x * dx + z * dz);
            assert(upstreamProgress > 0.25f * dt && upstreamProgress < dt);
        }
    }

    // Original swim ticks resume sinking when Space is released or rise is cooling down.
    float swimVelocity = swimmingTickVelocity(0.0f, true, true);
    assert(swimVelocity == 0.5f);
    for (int i = 0; i < 20; ++i)
        swimVelocity = swimmingTickVelocity(swimVelocity, true, true);
    assert(swimVelocity == 0.5f);
    swimVelocity = swimmingTickVelocity(swimVelocity, true, false);
    assert(swimVelocity == -0.25f);
    assert(swimmingTickVelocity(0.0f, false, true) > 0.0f); // Push off the bottom.

    // Waterfall pull is independent of upward pulses and integrates across frame rates.
    float fallingVelocity = 0.0f, fineFallingVelocity = 0.0f, fineFall = 0.0f;
    float fall = advanceCurrent(-SWIM_FALLING_CURRENT_SPEED, 1.0f, fallingVelocity, 12.0f);
    for (int i = 0; i < 100; ++i)
        fineFall += advanceCurrent(-SWIM_FALLING_CURRENT_SPEED, 0.01f, fineFallingVelocity, 12.0f);
    assert(fall < 0.0f && fallingVelocity < 0.0f);
    assert(std::abs(fall - fineFall) < 0.0001f);
    assert(std::abs(fallingVelocity - fineFallingVelocity) < 0.0001f);
    std::cout << "Swimming current checks passed\n";
}
