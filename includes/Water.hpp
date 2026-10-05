#pragma once
#include "define.hpp"
#include <array>
#include <algorithm>
#include <cmath>

// Preserve the existing byte IDs (including falling '7') in chunk storage.
// Depth is a storage convention only: amount = 8 - depth, never a volume.
constexpr char WATER_FLOW_1 = '1';
constexpr char WATER_FLOW_2 = '2';
constexpr char WATER_FLOW_3 = '3';
constexpr char WATER_FLOW_4 = '4';
constexpr char WATER_FLOW_5 = '5';
constexpr char WATER_FLOW_6 = '6';
constexpr char WATER_FALLING = '7';
constexpr char WATER_FLOW_7 = '8';
constexpr char WATER_UNKNOWN = '?';
constexpr int WATER_MAX_DISTANCE = 7;
constexpr int WATER_TICK_DELAY = 5;
constexpr int WATER_SLOPE_DISTANCE = 4;
constexpr bool isWater(char b) {
    return b == WATER || (b >= WATER_FLOW_1 && b <= WATER_FLOW_7);
}
constexpr bool waterReplaceable(char b) {
    return b == AIR || isWater(b) || b == FLOWER_POPPY || b == FLOWER_DANDELION ||
        b == FLOWER_CYAN || b == FLOWER_SHORT_GRASS || b == FLOWER_DEAD_BUSH;
}
constexpr int waterDistance(char b) {
    return b == WATER || b == WATER_FALLING ? 0 : b == WATER_FLOW_7 ? 7 :
        (b >= WATER_FLOW_1 && b <= WATER_FLOW_6 ? b - WATER_FLOW_1 + 1 : 8);
}
constexpr int waterAmount(char b) { return 8 - waterDistance(b); }
constexpr char waterFromAmount(int amount) {
    return amount <= 0 ? AIR : amount == 8 ? WATER : amount == 1 ? WATER_FLOW_7 :
        WATER_FLOW_1 + 7 - amount;
}
constexpr bool waterSupport(char b) {
    return b != WATER_UNKNOWN && (!waterReplaceable(b) || b == WATER);
}

// Fifteenths are the renderer's existing four-bit corner format. Approximate
// amount / 9 here; falling is full strength, not a ninth horizontal level.
constexpr int waterHeight(char b) {
    return isWater(b) ? (waterAmount(b) * 15 + 4) / 9 : 0;
}

// Gradients use effective fluid depth, independently of surface quantization.
// An empty ledge samples water one block below. Falling water adds downward
// pull near a bank; the player controller supplies drag and tunable strength.
template<class Read>
std::array<float, 3> waterCurrent(Read read) {
    char block = read(0, 0, 0);
    if (!isWater(block)) return {};
    float x = 0.0f, z = 0.0f;
    bool bank = false;
    constexpr int offsets[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
    for (auto& offset : offsets) {
        char neighbor = read(offset[0], 0, offset[1]);
        float drop = 0.0f;
        if (isWater(neighbor))
            drop = float(waterAmount(block) - waterAmount(neighbor)) / 9.0f;
        else if (waterReplaceable(neighbor)) {
            char lower = read(offset[0], -1, offset[1]);
            if (isWater(lower)) drop = float(waterAmount(block)) / 9.0f -
                (float(waterAmount(lower)) / 9.0f - 8.0f / 9.0f);
        }
        bank = bank || !waterReplaceable(neighbor) ||
            !waterReplaceable(read(offset[0], 1, offset[1]));
        x += offset[0] * drop;
        z += offset[1] * drop;
    }
    float length = std::sqrt(x * x + z * z);
    if (length > 0.0f) { x /= length; z /= length; }
    float y = block == WATER_FALLING && bank ? -6.0f : 0.0f;
    length = std::sqrt(x * x + y * y + z * z);
    return length > 0.0f ? std::array<float, 3>{x / length, y / length, z / length} :
        std::array<float, 3>{};
}

// The mesher and swimming probes must use identical corner samples and rounding.
// read(x,y,z) reads a cell relative to the water block being sampled.
template<class Read>
std::array<int, 4> waterSurfaceCorners(Read read) {
    std::array<int, 4> heights{};
    for (int z = 0; z <= 1; ++z) for (int x = 0; x <= 1; ++x) {
        int total = 0, count = 0;
        bool full = false;
        for (int dz = -1; dz <= 0; ++dz) for (int dx = -1; dx <= 0; ++dx) {
            char b = read(x + dx, 0, z + dz);
            if (!isWater(b)) continue;
            full = full || isWater(read(x + dx, 1, z + dz));
            total += waterHeight(b);
            ++count;
        }
        heights[x + 2 * z] = full ? 15 : (count ? (total + count / 2) / count : waterHeight(WATER));
    }
    return heights;
}

inline float waterSurfaceHeight(const std::array<int, 4>& h, float x, float z) {
    // Match the top face's triangle strip, whose diagonal is x + z = 1.
    if (x + z <= 1.0f)
        return (h[0] + x * (h[1] - h[0]) + z * (h[2] - h[0])) / 15.0f;
    return (h[3] + (1.0f - x) * (h[2] - h[3]) +
            (1.0f - z) * (h[1] - h[3])) / 15.0f;
}
