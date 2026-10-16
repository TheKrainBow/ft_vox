#pragma once

#include "Frustum.hpp"
#include <tuple>

// Load the player's view first, progressing outward along the look direction.
// Distance is measured relative to that direction, rather than world Y, so
// looking down/up prioritizes the ground/ceiling just like looking to the sides.
// Generation and meshing share this key to preserve the visible loading order.
struct GenerationPriority {
    int shell;
    bool outsideView;
    int heightDistance;
    float distanceSquared;
    float negativeAlignment;
    glm::ivec3 cell;

    bool operator<(const GenerationPriority& other) const {
        return std::tie(outsideView, shell, distanceSquared, heightDistance, negativeAlignment,
                        cell.x, cell.y, cell.z) <
               std::tie(other.outsideView, other.shell, other.distanceSquared, other.heightDistance,
                        other.negativeAlignment, other.cell.x, other.cell.y, other.cell.z);
    }
};

inline GenerationPriority generationPriority(glm::ivec3 cell, glm::vec3 eye,
                                              glm::vec3 forward, const Frustum* frustum) {
    glm::vec3 minimum = glm::vec3(cell) * float(CHUNK_SIZE);
    glm::vec3 maximum = minimum + glm::vec3(CHUNK_SIZE);
    glm::vec3 offset = (minimum + maximum) * 0.5f - eye;
    float distanceSquared = glm::dot(offset, offset);
    float alignment = distanceSquared > 0.00001f ?
        glm::dot(offset / std::sqrt(distanceSquared), forward) : 1.0f;
    bool containsEye = eye.x >= minimum.x && eye.x <= maximum.x &&
                       eye.y >= minimum.y && eye.y <= maximum.y &&
                       eye.z >= minimum.z && eye.z <= maximum.z;
    bool visible = containsEye || (frustum ? frustum->aabbVisible(minimum, maximum) :
                                  alignment >= std::cos(glm::radians(70.0f)));
    int playerLayer = int(std::floor(eye.y / CHUNK_SIZE));
    // Boxes intersecting the look ray use their entry distance. This prevents
    // a farther box centered on the ray from skipping a nearer box at its edge.
    float entry = 0.0f, exit = std::numeric_limits<float>::max();
    bool intersectsRay = true;
    for (int axis = 0; axis < 3; ++axis) {
        if (std::abs(forward[axis]) < 0.00001f) {
            if (eye[axis] < minimum[axis] || eye[axis] > maximum[axis]) intersectsRay = false;
        } else {
            float a = (minimum[axis] - eye[axis]) / forward[axis];
            float b = (maximum[axis] - eye[axis]) / forward[axis];
            entry = std::max(entry, std::min(a,b));
            exit = std::min(exit, std::max(a,b));
        }
    }
    float viewDistanceSquared;
    if (intersectsRay && entry <= exit) {
        viewDistanceSquared = entry * entry;
    } else {
        float depth = std::max(0.0f, glm::dot(offset, forward));
        glm::vec3 nearest = glm::clamp(eye + forward * depth, minimum, maximum) - eye;
        depth = std::max(0.0f, glm::dot(nearest, forward));
        float lateralSquared = std::max(0.0f, glm::dot(nearest, nearest) - depth * depth);
        viewDistanceSquared = depth * depth + 16.0f * lateralSquared;
    }
    int shell = int(std::floor(std::sqrt(viewDistanceSquared) / CHUNK_SIZE));
    return {shell, !visible, std::abs(cell.y - playerLayer),
            viewDistanceSquared, -alignment, cell};
}
