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
    fill(*water, AIR);
    fill(*center.getSubChunk(2), AIR);
    fill(*adjacent, AIR);

    // A rectangular waterfall merges in both axes, on every exposed side.
    for (int y = 4; y < 20; ++y)
        for (int z = 8; z < 11; ++z)
            for (int x = 8; x < 12; ++x)
                water->setBlockLocal(x, y, z, WATER_FALLING);
    water->sendFacesToDisplay();
    for (int dir = 0; dir < 6; ++dir)
        assert(water->getTranspDirCounts()[dir] == 1);
    for (int packed : water->getTransparentVertices()) {
        uint32_t bits = uint32_t(packed);
        assert((bits & 0x80000000u) == 0);
        assert(((bits >> 25) & 0x7f) == T_WATER_FULL);
    }
    size_t index = 0;
    for (auto dir : {UP, DOWN, NORTH, SOUTH, EAST, WEST}) {
        uint32_t bits = uint32_t(water->getTransparentVertices()[index++]);
        int width = int((bits >> 15) & 31u) + 1;
        int height = int((bits >> 20) & 31u) + 1;
        assert(width == ((dir == EAST || dir == WEST) ? 3 : 4));
        assert(height == ((dir == UP || dir == DOWN) ? 3 : 16));
    }

    // Covered flowing cells are full-height too, regardless of flow distance.
    for (int y = 4; y < 19; ++y)
        for (int z = 8; z < 11; ++z)
            for (int x = 8; x < 12; ++x)
                water->setBlockLocal(x, y, z, WATER_FLOW_3);
    water->sendFacesToDisplay();
    for (int dir = 0; dir < 6; ++dir)
        assert(water->getTranspDirCounts()[dir] == 1);

    // Lower stream surfaces retain their quantized corner heights.
    fill(*water, AIR);
    water->setBlockLocal(8, 8, 8, WATER_FLOW_1);
    water->sendFacesToDisplay();
    int shaped = 0;
    for (int packed : water->getTransparentVertices()) {
        uint32_t bits = uint32_t(packed);
        if (bits & 0x80000000u) {
            ++shaped;
            assert(((bits >> 15) & 0xffffu) == 0x9999u);
        }
    }
    assert(shaped == 5); // Bottom remains a mergeable rectangle.
    std::cout << "Flowing water greedy meshing checks passed\n";
}
