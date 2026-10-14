#pragma once
#include "Water.hpp"

constexpr bool isLava(char block) { return block == LAVA; }
constexpr bool isLiquid(char block) { return isWater(block) || isLava(block); }
constexpr float LAVA_SWIM_SPEED_FACTOR = 0.5f;
constexpr float lavaOpacity(float waterAlpha) { return 1.0f-(1.0f-waterAlpha)*0.1f; }

// Static lava uses water's source/full heights. Flat corners match the
// separately tagged lava rectangles without blending neighboring water.
template<class Read>
std::array<int,4> lavaSurfaceCorners(Read read) {
    int height=isLava(read(0,1,0)) ? 15 : waterHeight(WATER);
    return {height,height,height,height};
}
template<class Read>
float lavaColumnSurface(Read read,float,float) {
    int top=0;
    while (top<1024 && isLava(read(0,top+1,0))) ++top;
    return top+waterHeight(WATER)/15.0f;
}
