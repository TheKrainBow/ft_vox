#include "ChunkLoader.hpp"
#include <cassert>
#include <chrono>
#include <iostream>

bool isWSL() { return false; }
struct WaterIntegrationTest {
    static bool stagedHasHighBlock(ChunkLoader& loader) {
        std::lock_guard<std::mutex> lock(loader._sharedDrawDataMutex);
        if (loader._solidStagedDataQueue.empty()) return true;
        for (auto origin:loader._solidStagedDataQueue.back()->ssboData)
            if (origin.y==1024) return true;
        assert(loader._solidStagedDataQueue.size()==1);
        return false;
    }
    static void waitDeleted(ChunkLoader& loader) {
        auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
        while (loader.getBlock({0,0},{8,1024,8})!=AIR || stagedHasHighBlock(loader)) {
            assert(std::chrono::steady_clock::now()<deadline);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
    static void sameMesh(const DisplayData& a, const DisplayData& b) {
        assert(a.vertexData==b.vertexData);
        assert(a.ssboData==b.ssboData);
        assert(a.drawMeta==b.drawMeta);
        assert(a.indirectBufferData.size()==b.indirectBufferData.size());
        for (size_t i=0;i<a.indirectBufferData.size();++i) {
            const auto& x=a.indirectBufferData[i];
            const auto& y=b.indirectBufferData[i];
            assert(x.count==y.count && x.instanceCount==y.instanceCount);
            assert(x.first==y.first && x.baseInstance==y.baseInstance);
        }
    }
    static void priorityAndSeams(ChunkLoader& loader, Camera& camera) {
        loader.loadChunk(-1,0,0,{0,0},1);
        loader.loadChunk(0,-1,0,{0,0},1);
        loader.loadChunk(-1,-1,0,{0,0},1);
        std::lock_guard<std::mutex> buildLock(loader._displayBuildMutex);
        loader.flushDirtyChunks();
        assert(loader.setBlock({0,0},{8,1024,8},STONE,true));
        loader.flushDirtyChunks();
        loader.markChunkDirty({0,0}); // Keep a full generation rebuild pending too.
        loader.markChunkDirty({-1,0});
        Raycaster ray(loader,camera);
        assert(ray.raycastDeleteOne({8.5f,1027,8.5f},{0,-1,0},5));
        auto start=std::chrono::steady_clock::now();
        loader.flushDirtyChunks(true);
        DisplayData solid,transparent;
        loader.buildFacesToDisplay(&solid,&transparent);
        for (auto origin:solid.ssboData) assert(origin.y!=1024);
        {
            std::lock_guard<std::mutex> dirtyLock(loader._dirtyMutex);
            // An edit must be publishable without consuming unrelated work,
            // and its column's outstanding full rebuild must not be lost.
            assert(loader._dirtyChunks.count({-1,0}));
            assert(loader._dirtyChunks.count({0,0}));
            assert(loader._urgentLayers.empty());
        }
        std::cout << "Priority edit mesh ready with background backlog in "
                  << std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()
                  << " ms\n";
        loader.flushDirtyChunks();
        // Compare partial rebuilds with a full rebuild at horizontal, vertical,
        // negative-coordinate and liquid corner seams, for insertion and deletion.
        for (auto cell : {glm::ivec3(0,1024,0),glm::ivec3(-1,1023,-1),
                          glm::ivec3(0,1055,0),glm::ivec3(-1,1056,-1)}) {
            for (int dx=-1;dx<=1;++dx) for (int dy=-1;dy<=1;++dy)
                for (int dz=-1;dz<=1;++dz) {
                    auto p=cell+glm::ivec3(dx,dy,dz);
                    glm::ivec2 cp((int)std::floor(double(p.x)/CHUNK_SIZE),
                                  (int)std::floor(double(p.z)/CHUNK_SIZE));
                    assert(loader.setBlock(cp,p,WATER,true));
                }
            loader.flushDirtyChunks();
            glm::ivec2 cp((int)std::floor(double(cell.x)/CHUNK_SIZE),
                          (int)std::floor(double(cell.z)/CHUNK_SIZE));
            for (BlockType block : std::array<BlockType,5>{STONE,AIR,LAVA,WATER_FLOW_3,AIR}) {
                assert(loader.setBlockOrQueue(cp,cell,block,true));
                loader.flushDirtyChunks(true);
                DisplayData partialSolid,partialTransparent,fullSolid,fullTransparent;
                loader.buildFacesToDisplay(&partialSolid,&partialTransparent);
                for (auto pos : {glm::ivec2(0,0),glm::ivec2(-1,0),
                                 glm::ivec2(0,-1),glm::ivec2(-1,-1)})
                    loader.getChunk(pos)->sendFacesToDisplay();
                loader.buildFacesToDisplay(&fullSolid,&fullTransparent);
                sameMesh(partialSolid,fullSolid);
                sameMesh(partialTransparent,fullTransparent);
            }
        }
        // Background builds also make bounded progress and retain their remainder.
        loader.flushDirtyChunks();
        loader.markChunkDirty({0,0});
        loader.markChunkDirty({-1,0});
        loader.flushDirtyChunks(true);
        {
            std::lock_guard<std::mutex> dirtyLock(loader._dirtyMutex);
            assert(loader._dirtyChunks.size()==1);
        }
        loader.flushDirtyChunks(true);
        {
            std::lock_guard<std::mutex> dirtyLock(loader._dirtyMutex);
            assert(loader._dirtyChunks.empty());
        }
        std::cout << "Partial meshes match full rebuilds across block and liquid seams\n";
    }
    static void run(ChunkLoader& loader, Camera& camera, ThreadPool& pool) {
        loader.loadChunk(0,0,0,{0,0},1);
        assert(loader.setBlock({0,0},{8,1024,8},STONE,true));
        loader.rebuildDisplayDataNow();
        assert(stagedHasHighBlock(loader));
        Raycaster ray(loader,camera);
        std::promise<void> firstStarted,secondStarted,release;
        auto released=release.get_future().share();
        auto a=pool.enqueue([&] { firstStarted.set_value(); released.wait(); });
        auto b=pool.enqueue([&] { secondStarted.set_value(); released.wait(); });
        firstStarted.get_future().wait(); secondStarted.get_future().wait();
        auto start=std::chrono::steady_clock::now();
        assert(ray.raycastDeleteOne({8.5f,1027,8.5f},{0,-1,0},5));
        waitDeleted(loader); // Terrain workers remain blocked until after publication.
        auto elapsed=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
        release.set_value(); a.get(); b.get();
        // A long world rebuild must queue an edit without blocking its caller.
        assert(loader.setBlock({0,0},{8,1024,8},STONE,true));
        loader.rebuildDisplayDataNow();
        std::promise<void> locked,unlock;
        auto unlocked=unlock.get_future();
        std::thread generation([&] {
            std::lock_guard<std::recursive_mutex> lock(loader._blockEditMutex);
            locked.set_value(); unlocked.wait();
        });
        locked.get_future().wait();
        assert(ray.raycastDeleteOne({8.5f,1027,8.5f},{0,-1,0},5));
        assert(loader.getBlock({0,0},{8,1024,8})==STONE);
        unlock.set_value(); generation.join();
        waitDeleted(loader);
        priorityAndSeams(loader,camera);
        std::cout << "Edits replace staged meshes with terrain workers saturated (" << elapsed
                  << " ms); busy-world edits queue without blocking and publish after unlock\n";
    }
};
int main() {
    Camera camera;
    ThreadPool pool(2);
    Chrono chrono;
    std::atomic_bool running(true);
    std::mutex drawMutex;
    std::queue<DisplayData*> solids,transparent;
    CaveWaterSettings water; water.caveWaterFrequency=0;
    LavaSettings lava; lava.caveFrequency=0; lava.surfaceChance=0;
    ChunkLoader loader(42,camera,pool,chrono,&running,drawMutex,solids,transparent,water,lava);
    WaterIntegrationTest::run(loader,camera,pool);
}
