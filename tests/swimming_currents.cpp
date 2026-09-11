#include "Swimming.hpp"
#include "Water.hpp"
#include <cassert>
#include <iostream>

int main() {
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
    assert(std::abs(upstreamX + 2.0f) < 0.0001f && sidewaysZ == 3.0f);
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

    for (bool rise : {false, true}) {
        float stillVelocity = 0.0f, fallingVelocity = 0.0f;
        float stillY = advanceSwimming(0.0f, 10.0f, rise, 0.1f, stillVelocity);
        float fallingY = advanceSwimming(0.0f, 10.0f, rise, 0.1f, fallingVelocity,
                                        -SWIM_FALLING_CURRENT_SPEED);
        assert(fallingY < stillY && fallingVelocity < stillVelocity);
        assert(rise ? fallingY > 0.0f : fallingY < 0.0f);
    }
    std::cout << "Swimming current checks passed\n";
}
