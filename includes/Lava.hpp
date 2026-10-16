#pragma once
#include "Water.hpp"

constexpr char LAVA_FLOW_1=1, LAVA_FLOW_2=2, LAVA_FLOW_3=3, LAVA_FLOW_4=4,
    LAVA_FLOW_5=5, LAVA_FLOW_6=6, LAVA_FLOW_7=7, LAVA_FALLING=8;
constexpr int LAVA_TICK_DELAY = WATER_TICK_DELAY * 3;
constexpr bool isLava(char block) { return block == LAVA || (block>=LAVA_FLOW_FIRST && block<=LAVA_FLOW_LAST); }
constexpr char lavaAsWater(char b) {
    return b==LAVA ? WATER : b==LAVA_FALLING ? WATER_FALLING :
        (b>=LAVA_FLOW_1 && b<=LAVA_FLOW_7) ? waterFromAmount(8-b) : b;
}
constexpr char waterAsLava(char b) {
    return b==WATER ? LAVA : b==WATER_FALLING ? LAVA_FALLING :
        isWater(b) ? char(waterDistance(b)) : b;
}
constexpr int lavaHeight(char b) { return isLava(b) ? waterHeight(lavaAsWater(b)) : 0; }
constexpr bool isLiquid(char block) { return isWater(block) || isLava(block); }
constexpr float LAVA_SWIM_SPEED_FACTOR = 0.5f;
constexpr float lavaOpacity(float waterAlpha) { return 1.0f-(1.0f-waterAlpha)*0.1f; }

// Flat per-level lava rectangles preserve greedy meshing for slow flows.
template<class Read>
std::array<int,4> lavaSurfaceCorners(Read read) {
    int height=isLava(read(0,1,0)) ? 15 : lavaHeight(read(0,0,0));
    return {height,height,height,height};
}
template<class Read>
float lavaColumnSurface(Read read,float,float) {
    int top=0;
    while (top<1024 && isLava(read(0,top+1,0))) ++top;
    return top+lavaHeight(read(0,top,0))/15.0f;
}
