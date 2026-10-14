#pragma once
#include <array>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <cmath>
#include <algorithm>

using LavaEmitter = std::array<float,4>;
using LavaLightCell = std::array<int32_t,4>;
inline uint32_t lavaLightHash(int x,int y,int z) {
    uint32_t h=uint32_t(x)*73856093u ^ uint32_t(y)*19349663u ^ uint32_t(z)*83492791u;
    h^=h>>16; h*=0x7feb352du; h^=h>>15;
    return h;
}
inline float lavaLightAtDistance(float distance) {
    float t=std::clamp(distance/4.0f,0.0f,1.0f);
    return 0.85f*(1.0f-t*t*(3.0f-2.0f*t));
}
struct LavaLightGrid {
    std::vector<LavaLightCell> cells;
    std::vector<LavaEmitter> emitters;
    int mask=0;
};
struct LavaLightPositionHash {
    size_t operator()(const std::array<int,3>& p) const {
        return lavaLightHash(p[0],p[1],p[2]);
    }
};
inline LavaLightGrid buildLavaLightGrid(const std::vector<LavaEmitter>& emitters) {
    LavaLightGrid grid;
    grid.emitters=emitters;
    if (emitters.empty()) { grid.cells.resize(1); grid.emitters.resize(1); return grid; }
    std::unordered_map<std::array<int,3>,std::pair<float,int>,LavaLightPositionHash> nearest;
    nearest.reserve(emitters.size()*16);
    for (size_t i=0;i<emitters.size();++i) {
        const auto& e=emitters[i];
        int cx=int(std::floor(e[0])),cy=int(std::floor(e[1])),cz=int(std::floor(e[2]));
        for (int z=cz-5;z<=cz+5;++z) for (int y=cy-5;y<=cy+5;++y) for (int x=cx-5;x<=cx+5;++x) {
            float dx=x+0.5f-e[0],dy=y+0.5f-e[1],dz=z+0.5f-e[2];
            float distance=dx*dx+dy*dy+dz*dz;
            if (distance>4.9f*4.9f) continue; // Include interpolation neighbors at the four-block boundary.
            auto key=std::array<int,3>{x,y,z};
            auto [it,inserted]=nearest.try_emplace(key,distance,int(i)+1);
            if (!inserted && distance<it->second.first) it->second={distance,int(i)+1};
        }
    }
    size_t capacity=2;
    while (capacity<nearest.size()*4) capacity*=2;
    for (;;) {
        grid.cells.assign(capacity,{});
        bool fits=true;
        for (auto entry:nearest) {
            auto p=entry.first;
            size_t index=lavaLightHash(p[0],p[1],p[2])&(capacity-1);
            int probe=0;
            while (probe<16 && grid.cells[index][3]) { index=(index+1)&(capacity-1); ++probe; }
            if (probe==16) { fits=false; break; }
            grid.cells[index]={p[0],p[1],p[2],entry.second.second};
        }
        if (fits) break;
        capacity*=2;
    }
    grid.mask=int(capacity-1);
    return grid;
}
