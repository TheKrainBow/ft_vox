#include "LavaGenerator.hpp"
#include "LavaLighting.hpp"
#include "WaterSimulation.hpp"
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
    LavaGenerator reverseCached(42,ls);
    Positions reverseHot;
    for (int z=96;z>=-128;z-=32) for (int x=96;x>=-128;x-=32)
    for (int y=56;y>=8;y-=16) {
        auto part=positions(reverseCached.caves({x,y,z},{x+32,std::min(y+16,64),z+32},basins,water,true));
        reverseHot.insert(part.begin(),part.end());
    }
    assert(reverseHot==hot);
    CaveWaterSettings dry; dry.caveWaterFrequency=0;
    CaveWaterGenerator noWater(42,dry);
    LavaSettings springSettings=ls; springSettings.springChance=100;
    LavaGenerator springLava(42,springSettings);
    auto wall=[](CaveWaterPos p) {
        return p.y>16 && local(p.x)<8 ? CaveWaterVoxel::Air : CaveWaterVoxel::Solid;
    };
    auto sources=springLava.caves(min,max,wall,noWater);
    // The mixed generator may also choose a floor pocket below the cave.
    sources.erase(std::remove_if(sources.begin(),sources.end(),[](CaveWaterPos p) { return p.y<=16; }),sources.end());
    assert(!sources.empty());
    for (auto p:sources) {
        assert(wall(p)==CaveWaterVoxel::Solid);
        int exits=0;
        for (auto d:WATER_SIDES) exits+=wall({p.x+d[0],p.y,p.z+d[2]})==CaveWaterVoxel::Air;
        assert(exits==1 && wall({p.x,p.y+1,p.z})==CaveWaterVoxel::Solid && wall({p.x,p.y-1,p.z})==CaveWaterVoxel::Solid);
    }
    auto source=sources.front();
    WaterPos sourcePos{source.x,source.y,source.z};
    std::map<WaterPos,char> flowing{{sourcePos,LAVA}};
    auto read=[&](WaterPos p) {
        auto it=flowing.find(p);
        return it!=flowing.end() ? it->second : wall({p[0],p[1],p[2]})==CaveWaterVoxel::Air ? char(AIR) : char(STONE);
    };
    auto write=[&](WaterPos p,char b) { flowing[p]=b; };
    processLavaTick(read,write,sourcePos);
    WaterPos mouth{};
    for (auto d:WATER_SIDES) if (wall({source.x+d[0],source.y,source.z+d[2]})==CaveWaterVoxel::Air)
        mouth={source.x+d[0],source.y,source.z+d[2]};
    assert(read(mouth)==LAVA_FLOW_1);
    for (int y=source.y;y>17;--y) {
        processLavaTick(read,write,{mouth[0],y,mouth[2]});
        assert(read({mouth[0],y-1,mouth[2]})==LAVA_FALLING);
    }
    processLavaTick(read,write,{mouth[0],17,mouth[2]});
    assert(isLava(read({mouth[0],17,mouth[2]+1})));
    size_t sourceCount=0;
    for (auto entry:flowing) sourceCount+=entry.second==LAVA;
    assert(sourceCount==1);
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
    // Every shoreline has a full two-block stone bank and an open sky column.
    for (auto [position,block]:blocks) if (block==LAVA) {
        auto [x,y,z]=position;
        if (blocks.count({x,y+1,z}) && blocks.at({x,y+1,z})==LAVA) continue;
        assert(y==150); // Source replaces the surface block, rather than the block below.
        for (int dy=1;dy<=3;++dy) assert(blocks.at({x,y+dy,z})==AIR);
        for (int dz=-2;dz<=2;++dz) for (int dx=-2;dx<=2;++dx) {
            auto atLevel=blocks.find({x+dx,y,z+dz});
            assert(atLevel!=blocks.end());
            if (atLevel->second!=LAVA) assert(blocks.at({x+dx,150,z+dz})==STONE);
        }
    }
    // Uneven ground gets a flat rim at the lava's level, rather than tall
    // walls around a pool placed below the lowest surrounding column.
    auto unevenHeight=[](int x,int) { return 150+((x%4+4)%4); };
    auto unevenGeometry=[&](CaveWaterPos p) {
        return p.y<=unevenHeight(p.x,p.z) ? CaveWaterVoxel::Solid : CaveWaterVoxel::Air;
    };
    auto uneven=surfaceBlocks(lava.surface(min,max,unevenHeight,unevenGeometry,allow));
    size_t unevenSurface=0;
    for (auto [position,block]:uneven) if (block==LAVA) {
        auto [x,y,z]=position;
        auto above=uneven.find({x,y+1,z});
        if (above!=uneven.end() && above->second==LAVA) continue;
        ++unevenSurface;
        assert(y==152);
        for (int dy=1;dy<=3;++dy) assert(uneven.at({x,y+dy,z})==AIR);
        for (int dz=-2;dz<=2;++dz) for (int dx=-2;dx<=2;++dx) {
            assert(uneven.count({x+dx,y,z+dz}));
            assert(uneven.at({x+dx,y+1,z+dz})==AIR);
            char neighbor=uneven.at({x+dx,y,z+dz});
            assert(neighbor==LAVA || neighbor==STONE);
        }
    }
    assert(unevenSurface>0);
    assert(lava.surface(min,max,[](int,int) { return 111; },solid,allow).empty());
    assert(lava.surface(min,max,height,[](CaveWaterPos) { return CaveWaterVoxel::Air; },allow).empty());
    assert(lava.surface(min,max,height,solid,[](int,int) { return false; }).empty());
    LavaSettings rare; rare.caveFrequency=0;
    LavaGenerator rareLava(42,rare);
    auto rarePools=rareLava.surface({-2048,0,-2048},{2048,256,2048},height,solid,allow);
    rare.surfaceChance=1;
    auto common=LavaGenerator(42,rare).surface({-2048,0,-2048},{2048,256,2048},height,solid,allow);
    assert(!rarePools.empty() && rarePools.size()<common.size()/5);
    // Desert regions receive a valid pool without the rare-candidate roll.
    LavaSettings deserts; deserts.surfaceChance=0.000001f; deserts.caveFrequency=0;
    deserts.surfaceRegionSize=64;
    LavaGenerator desertLava(42,deserts);
    auto desertPlan=desertLava.surface(min,max,height,solid,allow,false,allow);
    std::set<std::pair<int,int>> coveredDeserts;
    for (auto p:desertPlan) if (p.block==LAVA)
        coveredDeserts.emplace(int(std::floor(p.position.x/64.0)),int(std::floor(p.position.z/64.0)));
    assert(coveredDeserts.size()==16);
    assert(desertLava.surface(min,max,height,solid,allow).empty());
    auto notDesert=[](int,int) { return false; };
    assert(desertLava.surface(min,max,height,solid,allow,false,notDesert).empty());
    // A small isolated desert still gets a pool; other biomes are left untouched.
    auto island=[](int x,int z) { return x>=16 && x<=48 && z>=16 && z<=48; };
    auto islandPlan=desertLava.surface({0,0,0},{64,256,64},height,solid,allow,false,island);
    assert(!islandPlan.empty());
    for (auto p:islandPlan) assert(island(p.position.x,p.position.z));
    // Search past obstructed sites instead of abandoning the desert's only roll.
    auto brokenGround=[](CaveWaterPos p) {
        return p.x>=64 && p.z>=64 ? CaveWaterVoxel::Solid : CaveWaterVoxel::Air;
    };
    LavaSettings wideDesert=deserts; wideDesert.surfaceRegionSize=192;
    assert(!LavaGenerator(42,wideDesert).surface({0,0,0},{192,256,192},height,brokenGround,allow,false,allow).empty());
    // Low deserts above the beach can host sealed pools, but ocean cannot.
    auto lowDesert=[](int,int) { return OCEAN_HEIGHT+6; };
    assert(!desertLava.surface(min,max,lowDesert,solid,allow,false,allow).empty());
    auto ocean=[](int,int) { return OCEAN_HEIGHT; };
    assert(desertLava.surface(min,max,ocean,solid,allow,false,allow).empty());
    // Cached plans agree with arbitrary tiles, including negative coordinates.
    LavaGenerator cachedDesert(42,deserts);
    auto desertBlocks=surfaceBlocks(desertPlan);
    assert(surfaceBlocks(cachedDesert.surface(min,max,height,solid,allow,true,allow))==desertBlocks);
    SurfaceBlocks desertTiles;
    for (int z=-128;z<128;z+=16) for (int x=-128;x<128;x+=16)
        for (auto p:cachedDesert.surface({x,0,z},{x+16,256,z+16},height,solid,allow,true,allow))
            desertTiles[{p.position.x,p.position.y,p.position.z}]=p.block;
    assert(desertTiles==desertBlocks);
    assert(desertLava.surface(min,max,height,[](CaveWaterPos) { return CaveWaterVoxel::Protected; },allow,false,allow).empty());
    LavaSettings disabled=deserts; disabled.surfaceChance=0;
    assert(LavaGenerator(42,disabled).surface(min,max,height,solid,allow,false,allow).empty());
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

    auto stream=[](int x,int y,int) {
        if (y!=0) return char(AIR);
        return x<0 ? char(LAVA_FLOW_1) : char(LAVA_FLOW_4);
    };
    auto corners=lavaSurfaceCorners(stream);
    assert((corners==std::array<int,4>{10,7,10,7}));
    for (float x:{0.0f,0.25f,0.9f,1.0f})
        for (float z:{0.1f,0.8f})
            assert(std::abs(lavaColumnSurface(stream,x,z)-waterSurfaceHeight(corners,x,z))<0.0001f);
    auto neighboring=[&](int x,int y,int z) { return stream(x-1,y,z); };
    auto left=lavaSurfaceCorners(neighboring);
    assert(left[1]==corners[0] && left[3]==corners[2]);
    auto mixed=[](int x,int y,int) {
        return y!=0 ? char(WATER) : x<0 ? char(WATER) : char(LAVA_FLOW_4);
    };
    for (int corner:lavaSurfaceCorners(mixed)) assert(corner==7);
    auto covered=[](int,int y,int) { return y<=1 ? char(LAVA_FLOW_4) : char(AIR); };
    for (int corner:lavaSurfaceCorners(covered)) assert(corner==15);
    assert(std::abs(lavaColumnSurface(covered,0.5f,0.5f)-(1+7.0f/15.0f))<0.0001f);

    std::cout << "Lava: " << hot.size() << " cave voxels, " << surfaceLava
        << " surface voxels; containment, water exclusion, borders, rarity and light bounds passed\n";
}
