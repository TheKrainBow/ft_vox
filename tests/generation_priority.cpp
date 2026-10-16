#include "ChunkLoader.hpp"
#include "GenerationPriority.hpp"
#include <cassert>
#include <iostream>

bool isWSL() { return false; }

static Frustum viewAt(glm::vec3 eye, glm::vec3 direction) {
    glm::vec3 up = std::abs(direction.y) > 0.99f ? glm::vec3(0,0,-1) : glm::vec3(0,1,0);
    return Frustum::fromVP(glm::perspective(glm::radians(75.0f),1.25f,0.1f,1000.0f) *
                           glm::lookAt(eye,eye+direction,up));
}
static void priorities() {
    const glm::vec3 forward(0,0,1);
    for (int layer : {2,7}) { // Cave and surface elevations.
        glm::vec3 eye(16,layer*32+16,16);
        auto view=viewAt(eye,forward);
        auto rank=[&](glm::ivec3 cell) { return generationPriority(cell,eye,forward,&view); };
        assert(rank({0,layer,1}) < rank({0,layer,3}));
        // Visible terrain wins over nearer terrain behind the player.
        assert(rank({0,layer,3}) < rank({0,layer,-1}));
        assert(rank({0,layer,1}) < rank({0,layer,-1}));
        assert(rank({0,layer,2}) < rank({0,layer+1,2}));
        // Height bias must not postpone a nearby floor until distant terrain.
        assert(rank({0,layer-1,2}) < rank({0,layer,6}));
        assert(!rank({0,layer,1}).outsideView);
        assert(rank({0,layer,-1}).outsideView);
    }
    for (int yaw=0;yaw<360;yaw+=30) {
        glm::vec3 eye(5,80,27);
        glm::vec3 heading(std::sin(glm::radians(float(yaw))),0,std::cos(glm::radians(float(yaw))));
        auto view=viewAt(eye,heading);
        std::vector<GenerationPriority> order;
        for (int z=-3;z<=3;++z) for (int x=-3;x<=3;++x)
            order.push_back(generationPriority({x,2,z},eye,heading,&view));
        std::sort(order.begin(),order.end());
        int lastShell=-1;
        bool outside=false;
        for (const auto& item:order) {
            if (item.outsideView!=outside) {
                assert(!outside); // Finish the view before the background.
                outside=true;
                lastShell=-1;
            }
            assert(item.shell>=lastShell);
            lastShell=item.shell;
        }
    }
    glm::vec3 eye(16,80,16);
    auto upView=viewAt(eye,{0,1,0});
    assert(generationPriority({0,4,0},eye,{0,1,0},&upView) <
           generationPriority({0,2,2},eye,{0,1,0},&upView));
    auto downView=viewAt(eye,{0,-1,0});
    assert(generationPriority({0,0,0},eye,{0,-1,0},&downView) <
           generationPriority({0,2,2},eye,{0,-1,0},&downView));
    // High-altitude views: a nearby off-screen layer and a higher visible
    // hillside must not displace the surface under the player's crosshair.
    for (bool useFrustum : {false,true}) {
        glm::vec3 highEye(16,656,16), down(0,-1,0);
        auto view=viewAt(highEye,down);
        auto rank=[&](glm::ivec3 cell) {
            return generationPriority(cell,highEye,down,useFrustum ? &view : nullptr);
        };
        assert(rank({0,3,0}) < rank({3,19,0}));
        assert(rank({0,3,0}) < rank({6,9,0}));
        assert(rank({0,4,0}) < rank({0,3,0})); // Near-to-far along the down ray.
        glm::vec3 lowEye(16,80,16), up(0,1,0);
        auto upFrustum=viewAt(lowEye,up);
        assert(generationPriority({0,12,0},lowEye,up,useFrustum ? &upFrustum : nullptr) <
               generationPriority({3,2,0},lowEye,up,useFrustum ? &upFrustum : nullptr));
        glm::vec3 side(1,0,0);
        auto sideFrustum=viewAt(lowEye,side);
        assert(generationPriority({8,2,0},lowEye,side,useFrustum ? &sideFrustum : nullptr) <
               generationPriority({0,3,0},lowEye,side,useFrustum ? &sideFrustum : nullptr));
    }
    // Re-ranking the same candidates follows changes of elevation and heading.
    assert(generationPriority({0,2,2},{16,80,16},forward,nullptr) <
           generationPriority({0,7,2},{16,80,16},forward,nullptr));
    assert(generationPriority({0,7,2},{16,240,16},forward,nullptr) <
           generationPriority({0,2,2},{16,240,16},forward,nullptr));
    assert(generationPriority({0,2,-2},eye,{0,0,-1},nullptr) <
           generationPriority({0,2,2},eye,{0,0,-1},nullptr));
    auto negative=generationPriority({-1,-1,-1},{-16,-16,-16},forward,nullptr);
    assert(negative.heightDistance==0 && !negative.outsideView);
}

struct WaterIntegrationTest {
    static void install(ChunkLoader& loader, Chunk* chunk, int layer) {
        auto generated=chunk->generateLayer(layer);
        std::lock_guard<std::mutex> buildLock(loader._displayBuildMutex);
        std::lock_guard<std::recursive_mutex> editLock(loader._blockEditMutex);
        chunk->installLayer(layer,std::move(generated));
        loader.waterChunkLoaded(*chunk,layer);
        loader.applyPendingFor(chunk->getPosition());
    }
    static void partial(ChunkLoader& loader, Camera& camera) {
        glm::ivec2 pos(2,0);
        auto* chunk=loader.prepareStreamingChunk(pos,1);
        assert(chunk->needsGeneration(0) && chunk->needsGeneration(2));
        install(loader,chunk,2);
        assert(chunk->hasBlockData() && !chunk->needsGeneration(2));
        assert(chunk->needsGeneration(0) && !chunk->getSubChunk(0));
        loader.rebuildDisplayDataNow();
        {
            std::lock_guard<std::mutex> drawLock(loader._sharedDrawDataMutex);
            assert(!loader._solidStagedDataQueue.empty());
            bool visible=false;
            for (auto origin:loader._solidStagedDataQueue.back()->ssboData)
                visible=visible || (origin.x==64 && origin.y==64 && origin.z==0);
            assert(visible); // Display a completed cave layer before its column completes.
        }
        // A liquid above an unfinished vertical neighbor must wait for terrain,
        // rather than treating missing data as air and creating that layer.
        assert(loader.setBlock(pos,{72,64,8},WATER,true));
        for (int tick=0;tick<6;++tick) loader.updateWaterTick();
        assert(!chunk->getSubChunk(1));
        glm::ivec3 future(72,40,8);
        assert(!loader.setBlockOrQueue(pos,future,STONE,true));
        loader.applyPendingFor(pos); // Polling must retain edit order while terrain is missing.
        assert(!loader.setBlockOrQueue(pos,future,LOG,true));
        loader.applyPendingFor(pos);
        assert(!chunk->getSubChunk(1));
        install(loader,chunk,1);
        assert(loader.getBlock(pos,future)==LOG);
        // A placeholder's address and pre-existing non-air edits survive terrain arrival.
        glm::ivec3 spill(72,104,8);
        assert(loader.setBlock(pos,spill,LOG,false));
        auto* placeholder=chunk->getSubChunk(3);
        install(loader,chunk,3);
        assert(chunk->getSubChunk(3)==placeholder);
        assert(loader.getBlock(pos,spill)==LOG);
        assert(chunk->needsGeneration(0));
        camera.setPos({-80,-80,-16});
        loader._renderDistance=1;
        loader.loadChunks(pos); // Resume this column from its generated layers.
        for (int layer=0;layer<=chunk->maxGenerationLayer();++layer)
            assert(!chunk->needsGeneration(layer));
        assert(loader.getBlock(pos,future)==LOG && loader.getBlock(pos,spill)==LOG);
        std::cout << "Partial layers display early; queued edits, placeholders and resumed generation passed\n";
    }
    static void publicationOrder(ChunkLoader& loader, Camera& camera) {
        const glm::vec3 eye(16,1040,16);
        camera.setPos(-eye);
        auto angles=camera.getAngles();
        camera.rotate(180.0f-angles.x,-angles.y,1.0);
        auto forward=camera.getDirection();
        auto view=viewAt(eye,forward);
        loader.setViewProj(view);
        std::vector<glm::ivec2> columns{{0,0},{0,1},{-1,1},{1,1},{0,2},{-1,2},{1,2}};
        for (auto pos:columns) loader.loadChunk(0,0,0,pos,1);
        std::lock_guard<std::mutex> buildLock(loader._displayBuildMutex);
        loader.flushDirtyChunks();
        std::vector<glm::ivec2> expected=columns;
        std::sort(expected.begin(),expected.end(),[&](auto a,auto b) {
            return generationPriority({a.x,32,a.y},eye,forward,&view) <
                   generationPriority({b.x,32,b.y},eye,forward,&view);
        });
        for (int pass=0;pass<2;++pass) {
            // Queue farther columns first, changing hash/insertion order each pass.
            std::reverse(columns.begin(),columns.end());
            for (auto pos:columns)
                assert(loader.setBlock(pos,{pos.x*32+8,1024,pos.y*32+8},pass ? WATER : STONE,false));
            std::set<std::array<int,2>> prefix;
            for (auto pos:expected) {
                loader.flushDirtyChunks(true);
                DisplayData solid,transparent;
                loader.buildFacesToDisplay(&solid,&transparent);
                std::set<std::array<int,2>> visible;
                for (const auto* data:{&solid,&transparent})
                    for (auto origin:data->ssboData)
                        if (origin.y==1024) visible.insert({int(origin.x)/32,int(origin.z)/32});
                prefix.insert({pos.x,pos.y});
                if (visible!=prefix) {
                    std::cerr << "pass=" << pass << " expected next=" << pos.x << "," << pos.y << " visible:";
                    for (auto p:visible) std::cerr << " " << p[0] << "," << p[1];
                    std::cerr << " expected:";
                    for (auto p:prefix) std::cerr << " " << p[0] << "," << p[1];
                    std::cerr << "\n";
                }
                assert(visible==prefix); // No farther spot may bypass an earlier mesh.
            }
            for (auto pos:columns)
                assert(loader.setBlock(pos,{pos.x*32+8,1024,pos.y*32+8},AIR,false));
            loader.flushDirtyChunks();
        }
        std::cout << "Solid/water display snapshots preserve outward order despite reversed dirty insertion\n";
    }
    static void lookingDownPublication(ChunkLoader& loader, Camera& camera) {
        const glm::vec3 eye(16,1552,16);
        camera.setPos(-eye);
        auto angles=camera.getAngles();
        camera.rotate(180.0f-angles.x,-90.0f-angles.y,1.0);
        auto forward=camera.getDirection();
        auto view=viewAt(eye,forward);
        loader.setViewProj(view);
        // Ground on the look ray, a higher visible hillside, and a nearer
        // high layer outside the view. World-height ordering picked the latter.
        std::vector<glm::ivec3> expected{{0,32,0},{6,41,0},{3,47,0}};
        for (auto cell:expected) loader.loadChunk(0,0,0,{cell.x,cell.z},1);
        std::lock_guard<std::mutex> buildLock(loader._displayBuildMutex);
        loader.flushDirtyChunks();
        for (auto it=expected.rbegin();it!=expected.rend();++it) {
            auto cell=*it;
            assert(loader.setBlock({cell.x,cell.z},cell*32+glm::ivec3(8,0,8),STONE,false));
        }
        std::set<std::array<int,3>> prefix,markers;
        for (auto cell:expected) markers.insert({cell.x,cell.y,cell.z});
        for (auto cell:expected) {
            loader.flushDirtyChunks(true);
            DisplayData solid,transparent;
            loader.buildFacesToDisplay(&solid,&transparent);
            std::set<std::array<int,3>> visible;
            for (auto origin:solid.ssboData) {
                std::array<int,3> key{int(origin.x)/32,int(origin.y)/32,int(origin.z)/32};
                if (markers.count(key)) visible.insert(key);
            }
            prefix.insert({cell.x,cell.y,cell.z});
            assert(visible==prefix);
        }
        for (auto cell:expected)
            assert(loader.setBlock({cell.x,cell.z},cell*32+glm::ivec3(8,0,8),AIR,false));
        loader.flushDirtyChunks();
        std::cout << "Looking down from flight displays aimed ground before higher side layers\n";
    }
    static void world(ChunkLoader& loader, Camera& camera, ThreadPool& pool) {
        camera.setPos({-16,-80,-16});
        loader._renderDistance=3;
        loader.loadChunks({0,0}); // Real global scheduler, with just one worker.
        assert(loader._displayedCount==9);
        for (int z=-1;z<=1;++z) for (int x=-1;x<=1;++x) {
            auto* chunk=loader.getChunk({x,z});
            assert(chunk && chunk->hasBlockData());
            for (int layer=0;layer<=chunk->maxGenerationLayer();++layer) {
                assert(!chunk->needsGeneration(layer));
                assert(chunk->getSubChunk(layer));
            }
            assert(chunk->getSubChunk(0)->getBlock({8,0,8})==BEDROCK);
        }
        // Verify generation order did not alter underground terrain.
        glm::ivec2 origin(0,0);
        auto* map=loader.getNoiseGenerator().getPerlinMap(origin);
        Chunk reference({0,0},map,loader.getCaveGenerator(),loader,pool,1);
        reference.loadBlocks();
        auto* actual=loader.getChunk({0,0});
        for (int z=0;z<32;++z) for (int x=0;x<32;++x)
            for (int y=0;y<int(map->heightMap[z*32+x])-7;++y)
                assert(actual->getSubChunk(y/32)->getBlock({x,y%32,z})==
                       reference.getSubChunk(y/32)->getBlock({x,y%32,z}));
        auto count=loader._chunksCount.load();
        camera.setPos({-16,-240,-16});
        loader.loadChunks({0,0});
        assert(loader._chunksCount==count); // Reuse generated layers after a height change.
        std::cout << "Global subchunk streaming with " << pool.workerCount()
                  << " worker(s), terrain parity and cache reuse passed\n";
    }
};
int main() {
    priorities();
    Camera camera;
    ThreadPool pool(1);
    Chrono chrono;
    std::atomic_bool running(true);
    std::mutex mutex;
    std::queue<DisplayData*> solids,transparent;
    CaveWaterSettings water; water.caveWaterFrequency=0;
    LavaSettings lava; lava.caveFrequency=0; lava.surfaceChance=0;
    ChunkLoader loader(42,camera,pool,chrono,&running,mutex,solids,transparent,water,lava);
    WaterIntegrationTest::world(loader,camera,pool);
    WaterIntegrationTest::partial(loader,camera);
    WaterIntegrationTest::publicationOrder(loader,camera);
    WaterIntegrationTest::lookingDownPublication(loader,camera);
    {
        ThreadPool parallelPool(4);
        std::mutex parallelMutex;
        std::queue<DisplayData*> parallelSolids,parallelTransparent;
        ChunkLoader parallelLoader(42,camera,parallelPool,chrono,&running,parallelMutex,
                                   parallelSolids,parallelTransparent,water,lava);
        WaterIntegrationTest::world(parallelLoader,camera,parallelPool);
    }
    std::cout << "Cave/surface height, pitch, view direction and distance priorities passed\n";
}
