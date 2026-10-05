#include "WaterSimulation.hpp"
#include <cassert>
#include <iostream>
#include <functional>

struct World {
    std::map<WaterPos, char> blocks;
    WaterUpdateQueue queue;
    bool conversion = true;
    std::function<char(WaterPos)> terrain = [](WaterPos p) { return p[1] <= 1 ? STONE : AIR; };
    char read(WaterPos p) const {
        auto it = blocks.find(p);
        return it == blocks.end() ? terrain(p) : it->second;
    }
    void set(WaterPos p, char b) {
        if (read(p) == b) return;
        blocks[p] = b;
        auto schedule = [&](WaterPos q) { if (isWater(read(q))) queue.schedule(q); };
        schedule(p);
        for (int d = 0; d < 4; ++d) schedule(waterSide(p,d));
        schedule(waterOffset(p,0,1,0));
        schedule(waterOffset(p,0,-1,0));
    }
    void tick() {
        for (auto p : queue.advance()) {
            auto r = [&](WaterPos q) { return read(q); };
            processWaterTick(r, [&](WaterPos q, char b) { set(q,b); }, p, conversion);
        }
    }
    void settle(int limit = 3000) {
        while (queue.size() && limit-- > 0) tick();
        assert(queue.size() == 0);
    }
    void noWater() const {
        for (auto entry : blocks) assert(!isWater(entry.second));
    }
};

int main() {
    for (int amount = 1; amount <= 8; ++amount)
        assert(waterAmount(waterFromAmount(amount)) == amount);
    assert(waterAmount(WATER_FALLING) == 8);
    WaterUpdateQueue queue;
    queue.schedule({0,2,0}); queue.schedule({0,2,0});
    assert(queue.size() == 1);
    for (int i = 0; i < 4; ++i) assert(queue.advance().empty());
    assert(queue.advance().size() == 1);

    // Deadline/budget pressure cannot drop work or postpone duplicate events.
    for (int x = 0; x < 600; ++x) queue.schedule({x,2,0});
    for (int i = 0; i < 4; ++i) assert(queue.advance().empty());
    queue.schedule({0,2,0});
    assert(queue.advance().size() == 512);
    assert(queue.advance().size() == 88 && queue.size() == 0);

    // Adjacent meshes must produce identical corner samples on their shared edge.
    auto surface = [](int x, int y, int z) -> char {
        if (y != 0 || z < 0 || z > 1) return AIR;
        return x == 0 ? WATER : x == 1 ? WATER_FLOW_3 : AIR;
    };
    auto left = waterSurfaceCorners(surface);
    auto right = waterSurfaceCorners([&](int x,int y,int z) { return surface(x+1,y,z); });
    assert(left[1] == right[0] && left[3] == right[2]);
    assert(left[0] > left[1] && right[0] > right[1]);
    auto covered = waterSurfaceCorners([](int,int y,int) { return y <= 1 ? WATER : AIR; });
    for (int height : covered) assert(height == 15);

    World flat;
    flat.set({0,2,0}, WATER);
    for (int i = 0; i < 4; ++i) flat.tick();
    assert(flat.read({1,2,0}) == AIR);
    flat.tick();
    assert(flat.read({1,2,0}) == WATER_FLOW_1);
    assert(flat.read({2,2,0}) == AIR);
    flat.settle();
    for (int x = -9; x <= 9; ++x) for (int z = -9; z <= 9; ++z) {
        int distance = std::abs(x) + std::abs(z);
        assert(flat.read({x,2,z}) == (distance <= 7 ? waterFromAmount(8-distance) : AIR));
    }
    for (int i = 0; i < 100; ++i) flat.tick();
    assert(flat.queue.size() == 0);
    flat.set({0,2,0}, AIR);
    assert(isWater(flat.read({1,2,0})));
    flat.settle(); flat.noWater();

    World fall;
    fall.set({0,42,0}, WATER);
    fall.settle();
    for (int y = 2; y < 42; ++y) assert(fall.read({0,y,0}) == WATER_FALLING);
    assert(fall.read({7,2,0}) == WATER_FLOW_7);
    assert(fall.read({8,2,0}) == AIR);
    assert(fall.read({1,41,0}) == AIR);
    fall.set({0,42,0}, AIR);
    fall.settle(); fall.noWater();

    for (int distance : {1,3,5,6}) {
        World cliff;
        cliff.set({distance,1,0}, AIR);
        cliff.set({0,2,0}, WATER);
        for (int i = 0; i < 5; ++i) cliff.tick();
        assert(cliff.read({1,2,0}) == WATER_FLOW_1);
        assert(cliff.read({-1,2,0}) == (distance <= 5 ? AIR : WATER_FLOW_1));
        assert(cliff.read({0,2,1}) == (distance <= 5 ? AIR : WATER_FLOW_1));
        cliff.settle();
    }
    World ties;
    ties.set({3,1,0}, AIR); ties.set({-3,1,0}, AIR);
    ties.set({0,2,0}, WATER);
    for (int i = 0; i < 5; ++i) ties.tick();
    assert(ties.read({1,2,0}) == WATER_FLOW_1 && ties.read({-1,2,0}) == WATER_FLOW_1);
    assert(ties.read({0,2,1}) == AIR);
    ties.settle();

    World wall;
    for (int z = -2; z <= 2; ++z) wall.set({1,2,z}, STONE);
    wall.set({0,2,0}, WATER); wall.settle();
    for (int z = -2; z <= 2; ++z) assert(wall.read({1,2,z}) == STONE);
    assert(isWater(wall.read({2,2,3})));
    wall.set({0,2,0}, AIR); wall.settle(); wall.noWater();

    for (bool conversion : {false,true}) {
        World renewable; renewable.conversion = conversion;
        renewable.set({-1,2,0}, WATER); renewable.set({1,2,0}, WATER);
        renewable.settle();
        assert(renewable.read({0,2,0}) == (conversion ? WATER : WATER_FLOW_1));
        renewable.set({0,2,0}, AIR); renewable.settle();
        assert(renewable.read({0,2,0}) == (conversion ? WATER : WATER_FLOW_1));
    }
    World pool;
    for (int x = 0; x < 2; ++x) for (int z = 0; z < 2; ++z) pool.set({x,2,z}, WATER);
    pool.settle(); pool.set({0,2,0}, AIR); pool.settle();
    assert(pool.read({0,2,0}) == WATER);

    World floating;
    floating.set({0,3,0}, STONE); floating.set({0,4,0}, WATER);
    floating.settle(); floating.set({0,3,0}, AIR); floating.settle();
    assert(floating.read({0,4,0}) == WATER && floating.read({0,3,0}) == WATER_FALLING);

    World intersect;
    intersect.conversion = false;
    intersect.set({-3,2,0}, WATER); intersect.set({0,2,-3}, WATER);
    intersect.settle();
    assert(waterAmount(intersect.read({0,2,0})) == 5);
    intersect.set({-3,2,0}, AIR); intersect.set({0,2,-3}, AIR);
    intersect.settle(); intersect.noWater();

    World plants;
    plants.set({1,2,0}, FLOWER_POPPY); plants.set({0,2,0}, WATER);
    plants.settle(); assert(plants.read({1,2,0}) == WATER_FLOW_1);

    // Source conversion requires known support, and takes priority over falling.
    auto local = [](WaterPos p) -> char {
        if (p == WaterPos{0,1,0}) return WATER_UNKNOWN;
        if (p == WaterPos{1,2,0} || p == WaterPos{-1,2,0}) return WATER;
        return AIR;
    };
    assert(computeWaterStateAt(local, {0,2,0}) == WATER_FLOW_1);
    auto supported = [&](WaterPos p) -> char {
        if (p == WaterPos{0,1,0} || p == WaterPos{0,3,0}) return WATER;
        return local(p);
    };
    assert(computeWaterStateAt(supported, {0,2,0}) == WATER);
    assert(computeWaterStateAt(supported, {0,2,0}, false) == WATER_FALLING);
    std::cout << "Water scheduling, range, waterfalls, routing, walls, decay, renewable sources and stabilization passed\n";
}
