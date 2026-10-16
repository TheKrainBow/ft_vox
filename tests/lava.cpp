#include "LavaGenerator.hpp"
#include "LavaLighting.hpp"
#include <cassert>
#include <set>
#include <map>
#include <tuple>
#include <iostream>
#include <atomic>
using Positions=std::set<std::tuple<int,int,int>>;
static Positions positions(const std::vector<CaveWaterPos>& v) {
    Positions out; for (auto p:v) out.emplace(p.x,p.y,p.z); return out;
}
static int local(int x) { return (x%16+16)%16; }
static CaveWaterVoxel basins(CaveWaterPos p) {
    bool pit=local(p.x)>=6 && local(p.x)<=10 && local(p.z)>=6 && local(p.z)<=10;
    return p.y>=20 || (pit && p.y>=18) ? CaveWaterVoxel::Air : CaveWaterVoxel::Solid;
}
int main() {
    CaveWaterSettings ws;
    ws.caveWaterFrequency=1; ws.caveWaterMaxY=64;
    ws.caveWaterPoolChance=1; ws.caveWaterPocketChance=0; ws.caveWaterSpringChance=0;
    CaveWaterGenerator water(42,ws);
    LavaSettings ls; ls.caveFrequency=1; ls.caveMaxY=64; ls.surfaceChance=1; ls.surfaceRegionSize=64;
    LavaGenerator lava(42,ls);
    CaveWaterPos min{-128,8,-128},max{128,64,128};
    auto wet=positions(water.generate({-192,8,-192},{192,64,192},basins));
    auto hot=positions(lava.caves(min,max,basins,water));
    assert(!wet.empty() && !hot.empty());
    for (auto [x,y,z]:hot) for (int dz=-2;dz<=2;++dz) for (int dy=-2;dy<=2;++dy) for (int dx=-2;dx<=2;++dx)
        assert(!wet.count({x+dx,y+dy,z+dz}));
    Positions split;
    for (int z=-128;z<128;z+=32) for (int x=-128;x<128;x+=32)
    for (int y=8;y<64;y+=16) {
        auto part=positions(lava.caves({x,y,z},{x+32,std::min(y+16,64),z+32},basins,water));
        split.insert(part.begin(),part.end());
    }
    assert(split==hot && hot==positions(lava.caves(min,max,basins,water)));
    LavaGenerator cachedLava(42,ls);
    size_t probes=0;
    auto counted=[&](CaveWaterPos p) { ++probes; return basins(p); };
    assert(hot==positions(cachedLava.caves(min,max,counted,water,true)));
    size_t firstProbes=probes;
    assert(firstProbes>0);
    assert(hot==positions(cachedLava.caves(min,max,counted,water,true)));
    assert(probes==firstProbes); // Reused lava needs no new water reservations.
    auto solid=[](CaveWaterPos) { return CaveWaterVoxel::Solid; };
    auto allow=[](int,int) { return true; };
    auto height=[](int,int) { return 150; };
    auto plan=lava.surface(min,max,height,solid,allow);
    assert(!plan.empty());
    std::map<std::tuple<int,int,int>,char> blocks,tiled;
    for (auto p:plan) blocks[{p.position.x,p.position.y,p.position.z}]=p.block;
    for (int z=-128;z<128;z+=32) for (int x=-128;x<128;x+=32)
        for (auto p:lava.surface({x,8,z},{x+32,64,z+32},height,solid,allow))
            tiled[{p.position.x,p.position.y,p.position.z}]=p.block;
    assert(blocks==tiled);
    // Complete surface plans are independent of output box/Y, and shared by workers.
    using SurfaceBlocks = std::map<std::tuple<int,int,int>,char>;
    auto surfaceBlocks=[](const std::vector<LavaPlacement>& plan) {
        SurfaceBlocks out;
        for (auto p:plan) out[{p.position.x,p.position.y,p.position.z}]=p.block;
        return out;
    };
    std::atomic_size_t surfaceProbes{0};
    auto countedHeight=[&](int,int) { ++surfaceProbes; return 150; };
    LavaGenerator cachedSurface(42,ls);
    std::vector<std::future<SurfaceBlocks>> jobs;
    for (int i=0;i<4;++i) jobs.push_back(std::async(std::launch::async,[&] {
        return surfaceBlocks(cachedSurface.surface(min,max,countedHeight,solid,allow,true));
    }));
    for (auto& job:jobs) assert(job.get()==blocks);
    size_t firstSurfaceProbes=surfaceProbes;
    assert(firstSurfaceProbes>0);
    SurfaceBlocks cachedTiled;
    for (int z=-128;z<128;z+=32) for (int x=-128;x<128;x+=32)
        for (auto p:cachedSurface.surface({x,128,z},{x+32,160,z+32},countedHeight,solid,allow,true))
            cachedTiled[{p.position.x,p.position.y,p.position.z}]=p.block;
    assert(cachedTiled==blocks && surfaceProbes==firstSurfaceProbes);
    // Rejected sites are cached too, and custom samplers remain uncached by default.
    LavaGenerator rejectedSurface(42,ls);
    auto denied=[&](int,int) { ++surfaceProbes; return false; };
    assert(rejectedSurface.surface(min,max,height,solid,denied,true).empty());
    size_t rejectedProbes=surfaceProbes;
    assert(rejectedSurface.surface(min,max,height,solid,denied,true).empty());
    assert(surfaceProbes==rejectedProbes);
    assert(surfaceBlocks(rejectedSurface.surface(min,max,height,solid,allow))==blocks);
    size_t surfaceLava=0,stone=0;
    for (auto entry:blocks) {
        stone+=entry.second==STONE;
        if (entry.second!=LAVA) continue;
        ++surfaceLava;
        auto [x,y,z]=entry.first;
        char below=blocks.at({x,y-1,z});
        assert(below==LAVA || below==STONE);
        for (auto d:{std::array<int,2>{1,0},{-1,0},{0,1},{0,-1}}) {
            auto it=blocks.find({x+d[0],y,z+d[1]});
            assert(it==blocks.end() || it->second==LAVA || it->second==STONE); // Unstamped base terrain is stone.
        }
    }
    assert(surfaceLava>0 && stone>surfaceLava);
    assert(lava.surface(min,max,[](int,int) { return 111; },solid,allow).empty());
    assert(lava.surface(min,max,height,[](CaveWaterPos) { return CaveWaterVoxel::Air; },allow).empty());
    assert(lava.surface(min,max,height,solid,[](int,int) { return false; }).empty());
    LavaSettings rare; rare.caveFrequency=0;
    LavaGenerator rareLava(42,rare);
    auto rarePools=rareLava.surface({-2048,0,-2048},{2048,256,2048},height,solid,allow);
    rare.surfaceChance=1;
    auto common=LavaGenerator(42,rare).surface({-2048,0,-2048},{2048,256,2048},height,solid,allow);
    assert(!rarePools.empty() && rarePools.size()<common.size()/5);
    assert(!isWater(LAVA) && !waterReplaceable(LAVA) && !waterSupport(LAVA) && isLiquid(LAVA));
    assert(std::abs(lavaOpacity(0.8f)-0.98f)<0.0001f);
    assert(std::abs(lavaOpacity(0.9f)-0.99f)<0.0001f);
    float last=1;
    for (float d:{0.0f,1.0f,2.0f,3.0f,4.0f}) {
        float light=lavaLightAtDistance(d);
        assert(light<last); last=light;
    }
    assert(lavaLightAtDistance(4)==0 && lavaLightAtDistance(5)==0);
    auto grid=buildLavaLightGrid({{-1.0f,32.8667f,-1.0f,0},{-2.0f,31.5f,-1.0f,0}});
    assert(grid.mask>0);
    for (auto cell:grid.cells) if (cell[3]) {
        size_t index=lavaLightHash(cell[0],cell[1],cell[2])&grid.mask;
        int probe=0;
        while (grid.cells[index]!=cell && probe++<16) index=(index+1)&grid.mask;
        assert(probe<16 && cell[3]<=int(grid.emitters.size()));
    }
    assert(buildLavaLightGrid({}).mask==0);
    auto step=[](int x,int y,int) { return y==0 || (x==1 && y==1) ? char(LAVA) : char(AIR); };
    for (int corner:lavaSurfaceCorners(step)) assert(corner==13);
    assert(std::abs(lavaColumnSurface(step,0.9f,0.5f)-13.0f/15.0f)<0.0001f);

    std::cout << "Lava: " << hot.size() << " cave voxels, " << surfaceLava
        << " surface voxels; containment, water exclusion, borders, rarity and light bounds passed\n";
}
