#include "ChunkLoader.hpp"
#include "Raycaster.hpp"
#include <cassert>
#include <iostream>

bool isWSL() { return false; }

int main() {
    Camera camera;
    camera.setPos({0, 0, 0});
    ThreadPool pool(2);
    Chrono chrono;
    std::atomic_bool running(true);
    std::mutex drawMutex;
    std::queue<DisplayData*> solids, transparent;
    ChunkLoader loader(42, camera, pool, chrono, &running, drawMutex, solids, transparent);
    loader.initSpawn();
    Raycaster raycaster(loader, camera);

    // A solid column in subchunk 0 must not repeat into the void below it.
    for (int y = 1; y < CHUNK_SIZE; ++y)
        assert(loader.setBlock({0, 0}, {2, y, 2}, STONE, false));
    for (int y = -2 * CHUNK_SIZE; y < 0; ++y) {
        assert(loader.getBlock({0, 0}, {2, y, 2}) == AIR);
        glm::ivec3 hit;
        glm::vec3 origin(1.5f, y + 0.5f, 2.5f);
        assert(!raycaster.raycastHit(origin, {1, 0, 0}, 5.0f, hit));
        assert(raycaster.raycastHitFetch(origin, {1, 0, 0}, 5.0f, hit, true) == AIR);
    }

    glm::ivec3 hit;
    assert(raycaster.raycastHit({1.5f, 1.5f, 2.5f}, {1, 0, 0}, 5.0f, hit));
    assert(hit == glm::ivec3(2, 1, 2));
    assert(loader.getBlock({0, 0}, {2, 0, 2}) == BEDROCK);
    // Empty space above the generated terrain remains a miss too.
    assert(!raycaster.raycastHit({1.5f, 10000.5f, 2.5f}, {1, 0, 0}, 5.0f, hit));
    std::cout << "Below-bedrock block lookup and raycast checks passed\n";
}
