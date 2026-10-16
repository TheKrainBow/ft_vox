#pragma once
#include "Water.hpp"

constexpr char LAVA_FLOW_1=1, LAVA_FLOW_2=2, LAVA_FLOW_3=3, LAVA_FLOW_4=4,
    LAVA_FLOW_5=5, LAVA_FLOW_6=6, LAVA_FLOW_7=7, LAVA_FALLING=8;
constexpr int LAVA_TICK_DELAY = WATER_TICK_DELAY * 2;
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

// Sources keep their regular flat surface. Flow corners share water's height
// averaging, with source edges anchored to the source surface to avoid cracks.
template<class Read>
std::array<int,4> lavaSurfaceCorners(Read read) {
    if (read(0,0,0)==LAVA) {
        int height=isLava(read(0,1,0)) ? 15 : lavaHeight(LAVA);
        return {height,height,height,height};
    }
    auto heights=waterSurfaceCorners([&](int x,int y,int z) {
        char block=read(x,y,z);
        return isLava(block) ? lavaAsWater(block) : char(AIR);
    });
    for (int z=0;z<=1;++z) for (int x=0;x<=1;++x) {
        bool source=false,full=false;
        for (int dz=-1;dz<=0;++dz) for (int dx=-1;dx<=0;++dx) {
            char block=read(x+dx,0,z+dz);
            source=source || block==LAVA;
            full=full || (isLava(block) && isLava(read(x+dx,1,z+dz)));
        }
        if (source) heights[x+2*z]=full ? 15 : lavaHeight(LAVA);
    }
    return heights;
}
template<class Read>
float lavaColumnSurface(Read read,float x,float z) {
    int top=0;
    while (top<1024 && isLava(read(0,top+1,0))) ++top;
    auto corners=lavaSurfaceCorners([&](int dx,int dy,int dz) {
        return read(dx,top+dy,dz);
    });
    return top+waterSurfaceHeight(corners,x,z);
}
