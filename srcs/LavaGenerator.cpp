#include "LavaGenerator.hpp"
#include <algorithm>
#include <cmath>
#include <set>
#include <array>

namespace {
uint64_t mix(uint64_t v) {
    v += 0x9e3779b97f4a7c15ULL;
    v = (v^(v>>30))*0xbf58476d1ce4e5b9ULL;
    v = (v^(v>>27))*0x94d049bb133111ebULL;
    return v^(v>>31);
}
int floorDiv(int a,int b) { return a/b-(a%b<0); }
CaveWaterSettings caveSettings(LavaSettings s) {
    CaveWaterSettings c;
    c.caveWaterFrequency=s.caveFrequency;
    c.caveWaterMinY=s.caveMinY; c.caveWaterMaxY=s.caveMaxY;
    c.caveWaterMinRadius=2; c.caveWaterMaxRadius=6;
    c.caveWaterPoolChance=0.65f; c.caveWaterPocketChance=0.35f; c.caveWaterSpringChance=0;
    return c;
}
}
LavaGenerator::LavaGenerator(uint64_t seed,LavaSettings settings)
    : m_seed(seed),m_settings(settings),m_caves(mix(seed^0x1a7aULL),caveSettings(settings)) {
    m_settings.surfaceRegionSize=std::clamp(settings.surfaceRegionSize,64,512);
    m_settings.surfaceChance=std::clamp(settings.surfaceChance,0.0f,1.0f);
}
std::vector<CaveWaterPos> LavaGenerator::caves(CaveWaterPos min,CaveWaterPos max,
    const CaveWaterGenerator::Sample& sample,const CaveWaterGenerator& water) const {
    if (max.y<=m_settings.caveMinY || min.y>=m_settings.caveMaxY || m_settings.caveFrequency<=0) return {};
    // Include every water feature that could touch a lava analysis volume. This
    // reservation is reconstructed from world geometry, including unloaded chunks.
    auto wet = water.generate({min.x-64,min.y-32,min.z-64},{max.x+64,max.y+32,max.z+64},sample);
    std::set<std::array<int,3>> reserved;
    for (auto p:wet) for (int z=-2;z<=2;++z) for (int y=-2;y<=2;++y) for (int x=-2;x<=2;++x)
        reserved.insert({p.x+x,p.y+y,p.z+z});
    // Recessed water springs can spill to a landing below their source.
    // Keep lava away from that runoff corridor and its horizontal reach too.
    for (auto p:wet) {
        if (sample(p)!=CaveWaterVoxel::Solid) continue;
        for (auto d:{std::array<int,2>{1,0},{-1,0},{0,1},{0,-1}}) {
            CaveWaterPos mouth{p.x+d[0],p.y,p.z+d[1]};
            if (sample(mouth)!=CaveWaterVoxel::Air) continue;
            int drop=1;
            while (drop<=16 && sample({mouth.x,mouth.y-drop,mouth.z})==CaveWaterVoxel::Air) ++drop;
            if (drop>16) continue;
            for (int y=mouth.y-drop;y<=mouth.y+2;++y)
            for (int z=-9;z<=9;++z) for (int x=-9;x<=9;++x)
                if (std::abs(x)+std::abs(z)<=9) reserved.insert({mouth.x+x,y,mouth.z+z});
        }
    }
    return m_caves.generate(min,max,[&](CaveWaterPos p) {
        return reserved.count({p.x,p.y,p.z}) ? CaveWaterVoxel::Protected : sample(p);
    });
}
std::vector<LavaPlacement> LavaGenerator::surface(CaveWaterPos min,CaveWaterPos max,
    const Height& height,const CaveWaterGenerator::Sample& sample,const Allowed& allowed) const {
    std::vector<LavaPlacement> result;
    int size=m_settings.surfaceRegionSize;
    if (m_settings.surfaceChance<=0) return result;
    for (int rz=floorDiv(min.z-7,size);rz<=floorDiv(max.z+6,size);++rz)
    for (int rx=floorDiv(min.x-7,size);rx<=floorDiv(max.x+6,size);++rx) {
        uint64_t h=mix(m_seed^mix(uint32_t(rx))^mix(uint64_t(uint32_t(rz))+739));
        auto random=[&](int salt) { return (mix(h+salt)>>11)*(1.0/9007199254740992.0); };
        if (random(0)>=m_settings.surfaceChance) continue;
        int cx=rx*size+8+int(random(1)*(size-16)), cz=rz*size+8+int(random(2)*(size-16));
        int radius=3+int(random(3)*3), bound=radius+2;
        if (cx+bound<min.x || cx-bound>=max.x || cz+bound<min.z || cz-bound>=max.z) continue;
        int low=height(cx,cz),high=low;
        bool valid=low>OCEAN_HEIGHT+12;
        for (int z=-bound;z<=bound && valid;++z) for (int x=-bound;x<=bound && valid;++x) {
            int surfaceY=height(cx+x,cz+z);
            low=std::min(low,surfaceY); high=std::max(high,surfaceY);
            if (high-low>3 || !allowed(cx+x,cz+z)) valid=false;
        }
        valid=valid && low>OCEAN_HEIGHT+12;
        int level=low-1;
        // Solid support and a sealed stone shell; no cave or water breakthrough.
        for (int z=-bound;z<=bound && valid;++z) for (int x=-bound;x<=bound && valid;++x)
        for (int y=level-2;y<=level && valid;++y)
            if (sample({cx+x,y,cz+z})!=CaveWaterVoxel::Solid) valid=false;
        if (!valid) continue;
        for (int z=-bound;z<=bound;++z) for (int x=-bound;x<=bound;++x) {
            float shape=float(x*x)/(radius*radius)+float(z*z)/(radius*radius*0.8f*0.8f)
                +0.06f*std::sin(x*1.1f+z*0.8f+float(h%31));
            float bankLimit=float((radius+2)*(radius+2))/(radius*radius);
            if (shape>bankLimit) continue;
            int wx=cx+x,wz=cz+z;
            if (wx<min.x || wx>=max.x || wz<min.z || wz>=max.z) continue;
            int surfaceY=height(wx,wz);
            for (int y=level-2;y<=surfaceY+2;++y) {
                char block=STONE;
                if (shape<1.0f) {
                    if (y>level) block=AIR;
                    else if (y==level || (y==level-1 && shape<0.65f)) block=LAVA;
                } else if (y>surfaceY) block=AIR;
                result.push_back({{wx,y,wz},block});
            }
        }
    }
    return result;
}
