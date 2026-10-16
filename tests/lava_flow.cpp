#include "WaterSimulation.hpp"
#include <cassert>
#include <iostream>

int main() {
    static_assert(LAVA_TICK_DELAY == 2*WATER_TICK_DELAY, "Lava must flow at half water speed");
    WaterUpdateQueue queue;
    WaterPos wet{1,11,1}, hot{3,11,1};
    queue.schedule(wet);
    queue.schedule(hot,LAVA_TICK_DELAY);
    for (int tick=1;tick<=LAVA_TICK_DELAY;++tick) {
        auto batch=queue.advance();
        if (tick==WATER_TICK_DELAY) assert(batch==std::vector<WaterPos>{wet});
        else if (tick==LAVA_TICK_DELAY) assert(batch==std::vector<WaterPos>{hot});
        else assert(batch.empty());
    }
    for (int distance=1;distance<=7;++distance) {
        char lava=char(distance);
        assert(isLava(lava) && !isWater(lava) && !waterSupport(lava));
        assert(waterAsLava(lavaAsWater(lava))==lava);
        assert(lavaHeight(lava)==waterHeight(waterFromAmount(8-distance)));
    }
    std::map<WaterPos,char> cells;
    auto read=[&](WaterPos p) {
        auto it=cells.find(p);
        return it!=cells.end() ? it->second : p[1]<=10 ? char(STONE) : char(AIR);
    };
    auto schedule=[&](WaterPos p) {
        if (isLiquid(read(p))) queue.schedule(p,isLava(read(p)) ? LAVA_TICK_DELAY : WATER_TICK_DELAY);
    };
    auto write=[&](WaterPos p,char b) {
        cells[p]=b; schedule(p);
        for (auto d:WATER_SIDES) schedule(waterOffset(p,d[0],0,d[2]));
        schedule(waterOffset(p,0,1,0)); schedule(waterOffset(p,0,-1,0));
    };
    auto settle=[&] {
        int limit=10000;
        while (queue.size() && --limit) {
            for (auto p:queue.advance()) processLavaTick(read,write,p);
        }
        assert(limit>0);
    };
    WaterPos source{-1,11,-1};
    write(source,LAVA);
    settle();
    for (int x=-9;x<=7;++x) for (int z=-9;z<=7;++z) {
        int distance=std::abs(x+1)+std::abs(z+1);
        assert(read({x,11,z})==(distance==0 ? LAVA : distance<=7 ? char(distance) : char(AIR)));
    }
    write(source,AIR); settle();
    for (auto cell:cells) assert(!isLava(cell.second));
    // A falling column reaches its landing and spreads without new sources.
    write({-1,15,-1},LAVA); settle();
    assert(read({-1,14,-1})==LAVA_FALLING && read({-1,11,-1})==LAVA_FALLING);
    assert(read({0,11,-1})==LAVA_FLOW_1);
    write({-1,15,-1},AIR); settle();
    for (auto cell:cells) assert(!isLava(cell.second));
    // Each liquid blocks the other and lava never creates two-source infinities.
    cells.clear(); cells[source]=LAVA; cells[{0,11,-1}]=WATER;
    processLavaTick(read,write,source);
    assert(read({0,11,-1})==WATER);
    cells.clear(); cells[source]=WATER; cells[{0,11,-1}]=LAVA;
    processWaterTick(read,write,source);
    assert(read({0,11,-1})==LAVA);
    cells.clear(); cells[{-1,11,0}]=LAVA; cells[{1,11,0}]=LAVA; cells[{0,11,0}]=LAVA_FLOW_1;
    processLavaTick(read,write,{0,11,0});
    assert(read({0,11,0})==LAVA_FLOW_1);
    std::cout << "Lava delay, spread, falls, decay, water separation and no source conversion passed\n";
}
