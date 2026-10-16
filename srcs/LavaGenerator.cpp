#include "LavaGenerator.hpp"
#include <algorithm>
#include <cmath>
#include <unordered_set>
#include <array>

namespace {
uint64_t mix(uint64_t v) {
    v += 0x9e3779b97f4a7c15ULL;
    v = (v^(v>>30))*0xbf58476d1ce4e5b9ULL;
    v = (v^(v>>27))*0x94d049bb133111ebULL;
    return v^(v>>31);
}
int floorDiv(int a,int b) { return a/b-(a%b<0); }
LavaSettings normalized(LavaSettings s) {
    s.caveFrequency=std::clamp(s.caveFrequency,0.0f,4.0f);
    s.caveMinY=std::max(2,s.caveMinY);
    s.surfaceRegionSize=std::clamp(s.surfaceRegionSize,64,512);
    s.surfaceChance=std::clamp(s.surfaceChance,0.0f,1.0f);
    s.deepFrequencyMultiplier=std::clamp(s.deepFrequencyMultiplier,0.0f,4.0f);
    s.upperFrequencyMultiplier=std::clamp(s.upperFrequencyMultiplier,0.0f,4.0f);
    s.springChance=std::max(0.0f,s.springChance);
    return s;
}
CaveWaterSettings caveSettings(LavaSettings s) {
    CaveWaterSettings c;
    c.caveWaterFrequency=s.caveFrequency;
    c.caveWaterMinY=s.caveMinY; c.caveWaterMaxY=s.caveMaxY;
    c.caveWaterMinRadius=2; c.caveWaterMaxRadius=16;
    c.caveWaterPoolChance=0.65f; c.caveWaterPocketChance=0.35f; c.caveWaterSpringChance=s.springChance;
    c.deepFrequencyMultiplier=s.deepFrequencyMultiplier; c.upperFrequencyMultiplier=s.upperFrequencyMultiplier;
    c.springClearanceRadius=9;
    return c;
}
}
LavaGenerator::LavaGenerator(uint64_t seed,LavaSettings settings)
    : m_seed(seed),m_settings(normalized(settings)),m_caves(mix(seed^0x1a7aULL),caveSettings(m_settings)) {}
std::vector<CaveWaterPos> LavaGenerator::caves(CaveWaterPos min,CaveWaterPos max,
    const CaveWaterGenerator::Sample& sample,const CaveWaterGenerator& water, bool cacheFeatures) const {
    if (max.y<=m_settings.caveMinY || min.y>=m_settings.caveMaxY || m_settings.caveFrequency<=0) return {};
    // Include every water feature that could touch a lava analysis volume. This
    // reservation is reconstructed from world geometry, including unloaded chunks.
    std::unordered_set<CaveWaterPos,CaveWaterPosHash> reserved;
    bool reservationsReady = false;
    auto reserveWater = [&] {
        // Cover complete lava analysis regions, not just the requested subchunk.
        // A cached plan must see every relevant water source in every caller.
        const auto& c=m_caves.settings();
        int size=c.regionSize, halo=8+c.maxDepth;
        CaveWaterPos low{floorDiv(min.x,size)*size-12,
            floorDiv(std::max(min.y-halo,c.caveWaterMinY-8),size)*size-32,
            floorDiv(min.z,size)*size-12};
        CaveWaterPos high{(floorDiv(max.x-1,size)+1)*size+12,
            (floorDiv(std::min(max.y+halo,c.caveWaterMaxY+8),size)+1)*size+32,
            (floorDiv(max.z-1,size)+1)*size+12};
        auto wet = water.generate(low,high,sample,cacheFeatures);
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
    };
    return m_caves.generate(min,max,[&](CaveWaterPos p) {
        // Cached/rejected lava features need no new water reservation analysis.
        if (!reservationsReady) { reserveWater(); reservationsReady = true; }
        return reserved.count({p.x,p.y,p.z}) ? CaveWaterVoxel::Protected : sample(p);
    },cacheFeatures);
}
std::vector<LavaPlacement> LavaGenerator::surface(CaveWaterPos min,CaveWaterPos max,
    const Height& height,const CaveWaterGenerator::Sample& sample,const Allowed& allowed, bool cacheFeatures) const {
    std::vector<LavaPlacement> result;
    int size=m_settings.surfaceRegionSize;
    if (m_settings.surfaceChance<=0) return result;
    for (int rz=floorDiv(min.z-8,size);rz<=floorDiv(max.z+7,size);++rz)
    for (int rx=floorDiv(min.x-8,size);rx<=floorDiv(max.x+7,size);++rx) {
        uint64_t h=mix(m_seed^mix(uint32_t(rx))^mix(uint64_t(uint32_t(rz))+739));
        auto random=[&](int salt) { return (mix(h+salt)>>11)*(1.0/9007199254740992.0); };
        if (random(0)>=m_settings.surfaceChance) continue;
        int cx=rx*size+10+int(random(1)*(size-20)), cz=rz*size+10+int(random(2)*(size-20));
        int radius=3+int(random(3)*4), bound=radius+2;
        if (cx+bound<min.x || cx-bound>=max.x || cz+bound<min.z || cz-bound>=max.z) continue;
        auto analyse = [&]() {
            std::vector<LavaPlacement> plan;
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
            if (!valid) return plan;
            auto shapeAt=[&](int x,int z) {
                return float(x*x)/(radius*radius)+float(z*z)/(radius*radius*0.8f*0.8f)
                    +0.06f*std::sin(x*1.1f+z*0.8f+float(h%31));
            };
            // Dilating the actual shoreline gives a complete two-block bank,
            // including concave corners, rather than a thin elliptical outline.
            std::array<float,17*17> shapes{};
            std::array<bool,17*17> bank{};
            int side=2*bound+1;
            for (int z=-bound;z<=bound;++z) for (int x=-bound;x<=bound;++x) {
                float shape=shapeAt(x,z);
                shapes[(z+bound)*side+x+bound]=shape;
                if (shape>=1.0f) continue;
                for (int nz=std::max(-bound,z-2);nz<=std::min(bound,z+2);++nz)
                for (int nx=std::max(-bound,x-2);nx<=std::min(bound,x+2);++nx)
                    bank[(nz+bound)*side+nx+bound]=true;
            }
            for (int z=-bound;z<=bound;++z) for (int x=-bound;x<=bound;++x) {
                int index=(z+bound)*side+x+bound;
                if (!bank[index]) continue;
                float shape=shapes[index];
                int wx=cx+x,wz=cz+z;
                int surfaceY=height(wx,wz);
                for (int y=level-2;y<=surfaceY+2;++y) {
                    char block=STONE;
                    if (shape<1.0f) {
                        if (y>level) block=AIR;
                        else if (y==level || (y==level-1 && shape<0.65f)) block=LAVA;
                    } else if (y>surfaceY) block=AIR;
                    plan.push_back({{wx,y,wz},block});
                }
            }
            return plan;
        };
        std::vector<LavaPlacement> uncached;
        const std::vector<LavaPlacement>* plan = &uncached;
        std::shared_future<std::vector<LavaPlacement>> future;
        if (!cacheFeatures) uncached = analyse();
        else {
            std::shared_ptr<std::promise<std::vector<LavaPlacement>>> producer;
            {
                std::lock_guard<std::mutex> lock(m_surfaceMutex);
                auto key = std::array<int,2>{rx,rz};
                auto found = m_surfaceFeatures.find(key);
                if (found != m_surfaceFeatures.end()) future = found->second;
                else {
                    producer = std::make_shared<std::promise<std::vector<LavaPlacement>>>();
                    future = producer->get_future().share();
                    if (m_surfaceFeatures.size() >= 512) m_surfaceFeatures.erase(m_surfaceFeatures.begin());
                    m_surfaceFeatures.emplace(key,future);
                }
            }
            if (producer) {
                try { producer->set_value(analyse()); }
                catch (...) { producer->set_exception(std::current_exception()); throw; }
            }
            plan = &future.get();
        }
        for (const auto& p : *plan)
            if (p.position.x>=min.x && p.position.x<max.x && p.position.z>=min.z && p.position.z<max.z)
                result.push_back(p);
    }
    return result;
}
