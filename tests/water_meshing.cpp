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
    CaveGenerator caves(42);
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

    // A solid ceiling leaves the lower water surface visible. Only water
    // above it culls the top, including across vertical subchunk borders.
    for (char current : {char(WATER), char(WATER_FLOW_1), char(WATER_FALLING)})
    for (int y : {8, CHUNK_SIZE - 1})
    for (char above : {char(AIR), char(STONE), char(DIRT), char(WATER),
                       char(WATER_FLOW_3), char(WATER_FALLING)}) {
        fill(*water, AIR);
        auto* upper = center.getSubChunk(2);
        fill(*upper, AIR);
        water->setBlockLocal(8, y, 8, current);
        if (y + 1 < CHUNK_SIZE) water->setBlockLocal(8, y + 1, 8, above);
        else upper->setBlockLocal(8, 0, 8, above);
        water->sendFacesToDisplay();
        bool topVisible = false;
        const auto& vertices = water->getTransparentVertices();
        for (int i = 0; i < water->getTranspDirCounts()[UP]; ++i)
            topVisible |= int((uint32_t(vertices[i]) >> 5) & 31u) == y;
        assert(topVisible == !isWater(above));
    }

    // Lava uses separate liquid material IDs, culls internal faces and retains
    // its top below solid ceilings while water remains a different material.
    for (char above : {char(AIR),char(STONE),char(WATER),char(LAVA)}) {
        fill(*water,AIR);
        fill(*center.getSubChunk(2),AIR);
        water->setBlockLocal(8,31,8,LAVA);
        center.getSubChunk(2)->setBlockLocal(8,0,8,above);
        water->sendFacesToDisplay();
        assert(water->getTranspDirCounts()[UP]==(above==LAVA ? 0 : 1));
        bool lavaMaterial=false;
        for (auto packed:water->getTransparentVertices()) {
            int material=(uint32_t(packed)>>25)&127;
            lavaMaterial= lavaMaterial || material==T_LAVA || material==T_LAVA_FULL;
        }
        assert(lavaMaterial);
    }

    // Coarse faces must inspect the whole touching patch, even at a 4:1 ratio.
    for (char flow : {char(LAVA_FLOW_1),char(LAVA_FLOW_4),char(LAVA_FLOW_7),char(LAVA_FALLING)}) {
        fill(*water,AIR);
        fill(*center.getSubChunk(2),AIR);
        water->setBlockLocal(8,31,8,flow);
        water->sendFacesToDisplay();
        assert(water->getTranspDirCounts()[UP]==1);
        uint32_t top=uint32_t(water->getTransparentVertices().front());
        if (flow==LAVA_FALLING) {
            assert(!(top&0x80000000u) && ((top>>25)&127)==T_LAVA);
        } else {
            assert(top&0x80000000u);
            uint32_t height=lavaHeight(flow);
            assert(((top>>15)&0xffffu)==height*0x1111u);
            assert(water->getLavaShapedCounts()[UP]==1);
        }
        center.getSubChunk(2)->setBlockLocal(8,0,8,LAVA_FLOW_1);
        water->sendFacesToDisplay();
        assert(water->getTranspDirCounts()[UP]==0);
    }

    // Flat pools and the full-height body of a lava fall remain greedy meshes.
    fill(*water,LAVA);
    fill(*center.getSubChunk(2),AIR);
    fill(*adjacent,LAVA);
    water->sendFacesToDisplay();
    assert(water->getTranspDirCounts()[UP]==1);
    assert(water->getLavaShapedCounts()[UP]==0);
    fill(*water,AIR);
    for (int y=4;y<20;++y)
        for (int z=8;z<11;++z)
            for (int x=8;x<12;++x)
                water->setBlockLocal(x,y,z,LAVA_FALLING);
    water->sendFacesToDisplay();
    assert(water->getTranspDirCounts()[DOWN]==1);
    for (auto dir:{NORTH,SOUTH,EAST,WEST})
        assert(water->getTranspDirCounts()[dir]==2);

    // A source stays flat while the adjacent stream slopes away from its edge.
    fill(*water,AIR);
    fill(*center.getSubChunk(2),AIR);
    water->setBlockLocal(8,8,8,LAVA);
    water->setBlockLocal(9,8,8,LAVA_FLOW_4);
    water->setBlockLocal(15,8,8,WATER_FLOW_1);
    water->sendFacesToDisplay();
    bool sourceTop=false,flowTop=false;
    for (int i=0;i<water->getTranspDirCounts()[UP];++i) {
        uint32_t top=uint32_t(water->getTransparentVertices()[i]);
        if ((top&31u)==8) {
            assert(!(top&0x80000000u) && ((top>>25)&127)==T_LAVA);
            sourceTop=true;
        }
        if ((top&31u)==9) {
            assert((top&0x80000000u) && ((top>>15)&0xffffu)==0x7d7du);
            flowTop=true;
        }
    }
    assert(sourceTop && flowTop);
    center.sendFacesToDisplay();
    std::vector<int> solidVertices,liquidVertices;
    std::vector<DrawArraysIndirectCommand> solidCommands,liquidCommands;
    std::vector<vec4> solidOrigins,liquidOrigins;
    std::vector<uint32_t> solidMeta,liquidMeta;
    center.snapshotDisplayData(solidVertices,solidCommands,solidOrigins,
        liquidVertices,liquidCommands,liquidOrigins,solidMeta,liquidMeta);
    assert(liquidCommands.size()==liquidMeta.size());
    size_t lavaInstances=0;
    for (size_t i=0;i<liquidCommands.size();++i) {
        const auto& command=liquidCommands[i];
        if (!(liquidMeta[i]&8u)) continue;
        lavaInstances+=command.instanceCount;
        for (uint32_t j=0;j<command.instanceCount;++j) {
            uint32_t bits=uint32_t(liquidVertices[command.baseInstance+j]);
            assert(bits&0x80000000u);
            assert((bits&31u)==9); // Water and regular source faces use other draws.
        }
    }
    assert(lavaInstances==4); // Top and three exposed sides, bottom remains flat.

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
