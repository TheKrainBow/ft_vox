#include "ChunkLoader.hpp"
#include <cassert>
#include <iostream>

bool isWSL() { return false; }

struct SpawnFloraTest {
	static void load(ChunkLoader &loader, glm::ivec2 pos) {
		loader.loadChunk(0, 0, 0, pos, 1);
	}
};

int main()
{
	Camera camera;
	ThreadPool pool(4);
	Chrono chrono;
	std::atomic_bool running(true);
	std::mutex drawMutex;
	std::queue<DisplayData*> solids, transparent;
	ChunkLoader loader(42, camera, pool, chrono, &running, drawMutex, solids, transparent);
	const auto spawn = camera.getChunkPosition(CHUNK_SIZE);
	loader.initSpawn();
	std::vector<std::tuple<glm::ivec2, int, glm::ivec3, BlockType>> plants;
	loader.fetchAndClearDiscoveredFlowers(plants);
	assert(!plants.empty());
	std::unordered_map<glm::ivec2, std::unordered_set<int>, ivec2_hash> displayed;
	loader.getDisplayedSubchunksSnapshot(displayed);
	for (const auto &plant : plants)
		assert(displayed.at(std::get<0>(plant)).count(std::get<1>(plant)));

	// Reproduce neighbor meshing before the column enters the displayed set.
	// The renderer would discard these early discoveries and the scan marks
	// would then prevent the real load from publishing the plants again.
	auto neighbor = spawn + glm::ivec2(1, 0);
	CaveGenerator caves(42);
	auto *map = loader.getNoiseGenerator().getPerlinMap(neighbor, 1);
	{
		Chunk early(neighbor, map, caves, loader, pool, 1);
		early.loadBlocks();
		early.sendFacesToDisplay();
		plants.clear();
		loader.fetchAndClearDiscoveredFlowers(plants);
		assert(plants.empty());
	}
	SpawnFloraTest::load(loader, neighbor);
	// Finish asynchronous builds before touching their staging queues.
	pool.joinThreads();
	// The splash polls readiness while workers remesh neighboring terrain.
	// These operations must not acquire the mesh/displayed-set locks in
	// opposite orders, or startup freezes before the first rendered frame.
	std::atomic_bool polling(true);
	std::thread readiness([&]() {
		while (polling.load()) assert(loader.hasRenderableChunks());
	});
	for (int i = 0; i < 16; ++i) {
		loader.getChunk(spawn)->sendFacesToDisplay();
		loader.getChunk(neighbor)->sendFacesToDisplay();
	}
	polling = false;
	readiness.join();
	plants.clear();
	loader.fetchAndClearDiscoveredFlowers(plants);
	size_t neighborPlants = 0;
	for (const auto &plant : plants) {
		if (std::get<0>(plant) != neighbor) continue;
		assert(loader.getBlock(neighbor, std::get<2>(plant)) == std::get<3>(plant));
		++neighborPlants;
	}
	assert(neighborPlants > 0);

	// A flat surface at the top of a subchunk puts its plants in the next
	// layer. That layer emits no terrain faces, but must still render plants.
	const int plantY = 32 * CHUNK_SIZE;
	const glm::ivec3 grass(spawn.x * CHUNK_SIZE + 8, plantY, spawn.y * CHUNK_SIZE + 8);
	const glm::ivec3 flower = grass + glm::ivec3(1, 0, 0);
	for (auto cell : {grass, flower})
		assert(loader.setBlock(spawn, cell - glm::ivec3(0, 1, 0), GRASS, false));
	assert(loader.setBlock(spawn, grass, FLOWER_SHORT_GRASS, false));
	assert(loader.setBlock(spawn, flower, FLOWER_POPPY, false));
	while (!solids.empty()) { delete solids.front(); solids.pop(); }
	while (!transparent.empty()) { delete transparent.front(); transparent.pop(); }
	loader.rebuildDisplayDataNow();
	loader.getDisplayedSubchunksSnapshot(displayed);
	assert(displayed.at(spawn).count(plantY / CHUNK_SIZE));
	for (const auto &plant : plants)
		assert(displayed.at(std::get<0>(plant)).count(std::get<1>(plant)));
	plants.clear();
	loader.fetchAndClearDiscoveredFlowers(plants);
	bool foundGrass = false, foundFlower = false;
	for (const auto &plant : plants) {
		if (std::get<2>(plant) == grass && std::get<3>(plant) == FLOWER_SHORT_GRASS) foundGrass = true;
		if (std::get<2>(plant) == flower && std::get<3>(plant) == FLOWER_POPPY) foundFlower = true;
	}
	assert(foundGrass && foundFlower);
	std::cout << "Spawn and neighbor flora discovery, early meshing, and plant-only visibility passed\n";
}
