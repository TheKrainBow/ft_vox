#pragma once
#include "define.hpp"
#include <array>
#include <algorithm>
#include <cmath>

// WATER remains the source type used by generation and the block picker.
constexpr char WATER_FLOW_1 = '1';
constexpr char WATER_FLOW_2 = '2';
constexpr char WATER_FLOW_3 = '3';
constexpr char WATER_FLOW_4 = '4';
constexpr char WATER_FLOW_5 = '5';
constexpr char WATER_FLOW_6 = '6';
constexpr char WATER_FALLING = '7';
constexpr int WATER_MAX_DISTANCE = 6;
constexpr bool isWater(char b) {
    return b == WATER || (b >= WATER_FLOW_1 && b <= WATER_FALLING);
}
constexpr bool waterReplaceable(char b) {
    return b == AIR || isWater(b) || b == FLOWER_POPPY || b == FLOWER_DANDELION ||
        b == FLOWER_CYAN || b == FLOWER_SHORT_GRASS || b == FLOWER_DEAD_BUSH;
}
constexpr int waterDistance(char b) {
    return b == WATER || b == WATER_FALLING ? 0 :
        (b >= WATER_FLOW_1 && b <= WATER_FLOW_6 ? b - WATER_FLOW_1 + 1 : 7);
}

// Pure local rule; unknown terrain must be supplied as solid by the caller.
inline char nextWater(char current, char above, char below,
                      const std::array<char, 4>& sides,
                      const std::array<char, 4>& sideFloors) {
    if (!waterReplaceable(current) || current == WATER) return current;
    int sources = std::count(sides.begin(), sides.end(), WATER);
    if (sources >= 2 && (!waterReplaceable(below) || below == WATER)) return WATER;
    if (isWater(above)) return WATER_FALLING;
    int distance = 7;
    for (int i = 0; i < 4; ++i) {
        // Sources always feed their banks. Streams fall before spreading sideways.
        if (sides[i] == WATER || (isWater(sides[i]) &&
            (!waterReplaceable(sideFloors[i]) || sideFloors[i] == WATER)))
            distance = std::min(distance, waterDistance(sides[i]) + 1);
    }
    if (distance <= WATER_MAX_DISTANCE) return WATER_FLOW_1 + distance - 1;
    return isWater(current) ? AIR : current;
}

// Quantized heights in fifteenths: the drop is steep near a source, then eases.
constexpr int waterHeight(char b) {
    constexpr int heights[] = {14, 9, 6, 4, 3, 2, 1};
    return b == WATER_FALLING ? 15 : (isWater(b) ? heights[waterDistance(b)] : 0);
}

// World-space direction: streams run away from their feeder towards lower water.
// Sources are still; falling cells pull straight down. Missing/solid neighbors
// do not contribute, so banks cannot create a sideways current.
template<class Read>
std::array<float, 3> waterCurrent(Read read) {
    char block = read(0, 0, 0);
    if (block == WATER_FALLING) return {0.0f, -1.0f, 0.0f};
    if (block < WATER_FLOW_1 || block > WATER_FLOW_6) return {};
    float x = 0.0f, z = 0.0f;
    constexpr int offsets[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
    // Prefer the feeder, including sources feeding FLOW_1. A falling outlet
    // also has distance zero and must not cancel the source's outward push.
    bool hasFeeder = false;
    for (auto& offset : offsets) {
        char neighbor = read(offset[0], 0, offset[1]);
        if (isWater(neighbor) && neighbor != WATER_FALLING &&
            waterDistance(neighbor) < waterDistance(block)) hasFeeder = true;
    }
    for (auto& offset : offsets) {
        char neighbor = read(offset[0], 0, offset[1]);
        if (!isWater(neighbor)) continue;
        if (hasFeeder && (neighbor == WATER_FALLING ||
            waterDistance(neighbor) >= waterDistance(block))) continue;
        float drop = float(waterDistance(neighbor) - waterDistance(block));
        x += offset[0] * drop;
        z += offset[1] * drop;
    }
    float length = std::sqrt(x * x + z * z);
    if (length == 0.0f) return {};
    return {x / length, 0.0f, z / length};
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
        heights[x + 2 * z] = full ? 15 : (count ? (total + count / 2) / count : 14);
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
