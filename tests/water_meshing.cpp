#include "ChunkLoader.hpp"
#include "SubChunk.hpp"
#include <cassert>
#include <iostream>

// Platform detection lives in main.cpp; no window is opened by this test.
bool isWSL() { return false; }

static void fill(SubChunk& sub, char block) {
    for (int y = 0; y < CHUNK_SIZE; ++y)
        for (int z = 0; z < CHUNK_SIZE; ++z)
            for (int x = 0; x < CHUNK_SIZE; ++x)
                sub.setBlockLocal(x, y, z, block);
}

int main() {
    Camera camera;
    ThreadPool pool(2);
    Chrono chrono;
    std::atomic_bool running(true);
    std::mutex drawMutex;
    std::queue<DisplayData*> solids, transparent;
    ChunkLoader loader(42, camera, pool, chrono, &running, drawMutex, solids, transparent);
    CaveGenerator caves(256, 0.01f, 0.05f, 0.6f, 0.6f, 42);
    PerlinMap map;
    map.heightMap = new double[CHUNK_SIZE * CHUNK_SIZE]();
    map.biomeMap = new Biome[CHUNK_SIZE * CHUNK_SIZE];
    std::fill_n(map.biomeMap, CHUNK_SIZE * CHUNK_SIZE, MOUNTAINS);
    map.heighest = 0;
    Chunk center({-2, -2}, &map, caves, loader, pool, 1);
    Chunk neighbor({-1, -2}, &map, caves, loader, pool, 1);
    center.loadBlocks();
    neighbor.loadBlocks();
    auto* water = center.getSubChunk(1);
    auto* adjacent = neighbor.getSubChunk(1);
    assert(water && adjacent);
    fill(*water, WATER);
    fill(*center.getSubChunk(2), AIR);
    fill(*adjacent, WATER);

    // Missing chunks must not create ocean walls in any horizontal direction.
    water->sendFacesToDisplay();
    for (auto dir : {NORTH, SOUTH, EAST, WEST})
        assert(water->getTranspDirCounts()[dir] == 0);

    center.setNorthChunk(&neighbor);
    center.setSouthChunk(&neighbor);
    center.setEastChunk(&neighbor);
    center.setWestChunk(&neighbor);
    water->sendFacesToDisplay();
    for (auto dir : {NORTH, SOUTH, EAST, WEST})
        assert(water->getTranspDirCounts()[dir] == 0);

    for (char block : {char(WATER_FLOW_1), char(WATER_FALLING), char(STONE)}) {
        fill(*adjacent, block);
        water->sendFacesToDisplay();
        for (auto dir : {NORTH, SOUTH, EAST, WEST})
            assert(water->getTranspDirCounts()[dir] == 0);
    }

    // A known empty neighbor exposes real water walls. The full-height section
    // and the lower surface strip each merge to one quad.
    fill(*adjacent, AIR);
    water->sendFacesToDisplay();
    for (auto dir : {NORTH, SOUTH, EAST, WEST})
        assert(water->getTranspDirCounts()[dir] == 2);
    bool foundFullHeight = false;
    for (int packed : water->getTransparentVertices()) {
        if (((uint32_t(packed) >> 25) & 0x7f) == T_WATER_FULL) {
            assert((uint32_t(packed) & 0x80000000u) == 0);
            foundFullHeight = true;
        }
    }
    assert(foundFullHeight);

    // Sloped water must retain its corner data instead of becoming a rectangle.
    fill(*water, AIR);
    fill(*center.getSubChunk(2), AIR);
    water->setBlockLocal(8, 8, 8, WATER_FLOW_1);
    water->sendFacesToDisplay();
    bool foundSlope = false;
    for (int packed : water->getTransparentVertices())
        foundSlope = foundSlope || (uint32_t(packed) & 0x80000000u);
    assert(foundSlope);

    // Coarse faces must inspect the whole touching patch, even at a 4:1 ratio.
    fill(*adjacent, WATER);
    for (auto dir : {NORTH, SOUTH, EAST, WEST}) {
        assert(!adjacent->isNeighborTransparent({0, 0, 0}, dir, WATER, 4));
        ivec3 hole = (dir == NORTH || dir == SOUTH) ? ivec3(3, 3, 0) : ivec3(0, 3, 3);
        adjacent->setBlockLocal(hole.x, hole.y, hole.z, AIR);
        assert(adjacent->isNeighborTransparent({0, 0, 0}, dir, WATER, 4));
        adjacent->setBlockLocal(hole.x, hole.y, hole.z, WATER);
    }
    // A finer viewer must still see an air cell in a coarser neighbor.
    SubChunk coarse({-1, 1, -2}, &map, caves, neighbor, loader, 4);
    fill(coarse, WATER);
    for (auto dir : {NORTH, SOUTH, EAST, WEST}) {
        assert(!coarse.isNeighborTransparent({0, 0, 0}, dir, WATER, 1));
        coarse.setBlockLocal(0, 0, 0, AIR);
        assert(coarse.isNeighborTransparent({0, 0, 0}, dir, WATER, 1));
        coarse.setBlockLocal(0, 0, 0, WATER);
    }
    std::cout << "Water border culling and greedy meshing checks passed\n";
}
