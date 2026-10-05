#include "ChunkLoader.hpp"
#include <cassert>
#include <iostream>

bool isWSL() { return false; }
struct WaterIntegrationTest {
    static Chunk* load(ChunkLoader& loader, glm::ivec2 p) {
        return loader.loadChunk(0,0,0,p,1);
    }
    static void tick(ChunkLoader& loader) { loader.stepWater(); }
    static void settle(ChunkLoader& loader) {
        int limit = 5000;
        while (loader.pendingWaterUpdates() && limit-- > 0) tick(loader);
        assert(!loader.pendingWaterUpdates());
    }
    static void run(ChunkLoader& loader) {
        load(loader, {0,0});
        loader._waterQueue = WaterUpdateQueue{};
        auto set = [&](int x, int y, int z, char b) {
            assert(loader.setBlock({int(std::floor(double(x)/CHUNK_SIZE)),0}, {x,y,z}, b, true));
        };
        auto read = [&](int x, int y, int z) {
            return loader.getBlock({int(std::floor(double(x)/CHUNK_SIZE)),0}, {x,y,z});
        };
        // A supported source waits at an absent negative-coordinate neighbor.
        for (int x = 0; x <= 16; ++x) for (int z = 3; z <= 29; ++z)
            set(x,1023,z,STONE);
        set(0,1024,16,WATER);
        settle(loader);
        assert(!loader._waterWaiting.empty());
        assert(read(1,1024,16) == AIR);
        // Loading the missing chunk must wake the original event without polling.
        load(loader, {-1,0});
        for (int x = -16; x < 0; ++x) for (int z = 3; z <= 29; ++z)
            set(x,1023,z,STONE);
        settle(loader);
        for (int x = -8; x <= 8; ++x) for (int dz = -8; dz <= 8; ++dz) {
            int distance = std::abs(x) + std::abs(dz);
            assert(read(x,1024,16+dz) == (distance <= 7 ? waterFromAmount(8-distance) : AIR));
        }
        assert(loader.getChunk({0,0})->getModified());
        assert(loader.getChunk({-1,0})->getModified());
        // Horizontal diagonal dependencies dirty both columns at the seam.
        assert(loader._dirtyChunks.count({0,0}) && loader._dirtyChunks.count({-1,0}));
        set(0,1024,16,AIR);
        settle(loader);
        for (int x = -8; x <= 8; ++x) for (int dz = -8; dz <= 8; ++dz)
            assert(!isWater(read(x,1024,16+dz)));
        // Modified columns survive eviction along with their exact byte states.
        assert(!loader.evictChunkAt({0,0}));
        load(loader, {2,0});
        loader._waterQueue.schedule({64,1024,16});
        loader._displayedChunks.erase({2,0});
        assert(loader.evictChunkAt({2,0}));
        settle(loader); // queued world coordinate remains safe after destruction
        load(loader, {2,0});
        settle(loader);
    }
};

int main() {
    Camera camera;
    ThreadPool pool(2);
    Chrono chrono;
    std::atomic_bool running(true);
    std::mutex drawMutex;
    std::queue<DisplayData*> solids, transparent;
    ChunkLoader loader(42,camera,pool,chrono,&running,drawMutex,solids,transparent);
    WaterIntegrationTest::run(loader);
    std::cout << "Water chunk-border wakeup, negative coordinates, mesh invalidation, cache and eviction checks passed\n";
}
