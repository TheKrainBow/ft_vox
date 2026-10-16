#include "ChunkLoader.hpp"
#include "Player.hpp"
#include <cassert>
#include <iostream>

bool isWSL() { return false; }
struct WaterIntegrationTest {
    static void immersion(ChunkManager& world, Camera& camera) {
        auto& loader = world._chunkLoader;
        load(loader, {-1,-1});
        for (int z=-3; z<=-1; ++z) for (int x=-3; x<=-1; ++x) {
            assert(loader.setBlock({-1,-1},{x,30,z},STONE,true));
            assert(loader.setBlock({-1,-1},{x,31,z},WATER,true));
            assert(loader.setBlock({-1,-1},{x,32,z},WATER,true));
            assert(loader.setBlock({-1,-1},{x,33,z},STONE,true));
        }
        Player player(camera,world);
        bool gravity = false;
        player.setGravity(gravity);
        camera.setPos({2.5f,-31.5f,2.5f});
        player.updatePlayerStates();
        assert(player.isUnderWater());
        assert(std::abs(player.underwaterSurfaceHeight()-(32.0f+13.0f/15.0f))<0.0001f);
        camera.setPos({2.5f,-32.8f,2.5f});
        player.updateWaterImmersion();
        assert(player.isUnderWater());
        camera.setPos({2.5f,-32.95f,2.5f});
        player.updateWaterImmersion();
        assert(!player.isUnderWater()); // Eyes above water, body still immersed.
        for (int z=-3; z<=-1; ++z) for (int x=-3; x<=-1; ++x)
            assert(loader.setBlock({-1,-1},{x,32,z},WATER_FLOW_3,true));
        camera.setPos({2.5f,-32.45f,2.5f});
        player.updateWaterImmersion();
        assert(player.isUnderWater());
        camera.setPos({2.5f,-32.6f,2.5f});
        player.updateWaterImmersion();
        assert(!player.isUnderWater());
        for (int z=-3; z<=-1; ++z) for (int x=-3; x<=-1; ++x)
        for (int y=31;y<=32;++y) assert(loader.setBlock({-1,-1},{x,y,z},LAVA,true));
        camera.setPos({2.5f,-32.65f,2.5f});
        player.updateWaterImmersion();
        assert(player.isUnderLava() && !player.isUnderWater());
        gravity=true;
        player.setGravity(gravity);
        float dt=0.01f;
        player.updateDeltaTime(dt);
        player.updatePlayerStates();
        assert(player._swimming && player._swimmingInLava);
        player.findMoveRotationSpeed();
        assert(std::abs(player._moveSpeed-MOVEMENT_SPEED*player._deltaTime*0.25f)<0.00001f);
        // Exercise the real vertical integration with the same swim velocity.
        auto vertical=[&](bool lava) {
            camera.setPos({2.5f,-32.65f,2.5f});
            player._fallSpeed=0.5f; player._falling=true; player._swimming=true; player._swimmingInLava=lava;
            player.updateDeltaTime(dt);
            auto pos=camera.getWorldPosition();
            int floor=30;
            player.updateFalling(pos,floor);
            return camera.getWorldPosition().y-pos.y;
        };
        assert(std::abs(vertical(true)*2-vertical(false))<0.00001f);
        gravity=false; player.setGravity(gravity);
        player.updatePlayerStates();
        assert(!player._swimming && player.isUnderLava());
        player.findMoveRotationSpeed();
        assert(std::abs(player._moveSpeed-MOVEMENT_SPEED*player._deltaTime)<0.00001f);
        camera.setPos({2.5f,-34,2.5f});
        player.updateWaterImmersion();
        assert(!player.isUnderLava() && !player.isUnderWater());
        for (int z=-3;z<=-1;++z) for (int x=-3;x<=-1;++x) {
            assert(loader.setBlock({-1,-1},{x,32,z},AIR,true));
            assert(loader.setBlock({-1,-1},{x,31,z},LAVA_FLOW_7,true));
        }
        camera.setPos({2.5f,-31.1f,2.5f});
        player.updateWaterImmersion();
        assert(player.isUnderLava());
        camera.setPos({2.5f,-31.2f,2.5f});
        player.updateWaterImmersion();
        assert(!player.isUnderLava()); // Thin flows use their rendered height.
    }
    static Chunk* load(ChunkLoader& loader, glm::ivec2 p) {
        return loader.loadChunk(0,0,0,p,1);
    }
    static void tick(ChunkLoader& loader) { loader.stepWater(); }
    static void busyGeneration(ChunkLoader& loader) {
        std::promise<void> locked, release;
        auto released=release.get_future();
        std::thread generation([&] {
            std::lock_guard<std::recursive_mutex> lock(loader._blockEditMutex);
            locked.set_value();
            released.wait();
        });
        locked.get_future().wait();
        size_t pending=loader.pendingWaterUpdates();
        // Must return without advancing/removing events while generation owns
        // the lock. This thread releases it only after the frame tick returns.
        loader.updateWaterTick();
        assert(loader.pendingWaterUpdates()==pending);
        release.set_value();
        generation.join();
    }
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
        busyGeneration(loader);
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
        // Lava uses the production scheduler across the negative chunk seam.
        for (int x=-8;x<=8;++x) for (int z=8;z<=24;++z) set(x,1151,z,STONE);
        set(0,1152,16,LAVA);
        for (int i=0;i<WATER_TICK_DELAY;++i) tick(loader);
        assert(read(-1,1152,16)==AIR && read(1,1152,16)==AIR);
        settle(loader);
        assert(read(-1,1152,16)==LAVA_FLOW_1 && read(1,1152,16)==LAVA_FLOW_1);
        assert(read(7,1152,16)==LAVA_FLOW_7 && read(8,1152,16)==AIR);
        set(0,1152,16,AIR);
        settle(loader);
        for (int x=-8;x<=8;++x) for (int z=8;z<=24;++z) assert(!isLava(read(x,1152,z)));
        // Lava lighting tracks faces and responds to source/neighbor edits.
        size_t baseEmitters=loader.lavaEmitters().size();
        for (int z=14;z<=18;++z) for (int x=6;x<=10;++x) set(x,1087,z,STONE);
        set(8,1088,16,AIR);
        uint64_t revision=loader.lavaLightRevision();
        set(8,1088,16,LAVA);
        assert(loader.lavaLightRevision()>revision);
        assert(loader.lavaEmitters().size()==baseEmitters+5);
        revision=loader.lavaLightRevision();
        set(9,1088,16,STONE);
        assert(loader.lavaLightRevision()>revision && loader.lavaEmitters().size()==baseEmitters+4);
        set(8,1088,16,AIR);
        assert(loader.lavaEmitters().size()==baseEmitters);
        // Horizontal diagonal dependencies dirty both columns at the seam.
        {
            std::lock_guard<std::mutex> displayLock(loader._displayBuildMutex);
            set(0,1024,16,AIR);
            std::lock_guard<std::mutex> dirtyLock(loader._dirtyMutex);
            assert(loader._dirtyChunks.count({0,0}) && loader._dirtyChunks.count({-1,0}));
        }
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
    ChunkManager world(42,&running,camera,chrono,pool);
    WaterIntegrationTest::immersion(world,camera);
    std::cout << "Water chunk-border wakeup, negative coordinates, mesh invalidation, cache and eviction checks passed\n";
}
