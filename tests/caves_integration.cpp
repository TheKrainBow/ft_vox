#include "ChunkLoader.hpp"
#include "SubChunk.hpp"
#include <cassert>
#include <chrono>
#include <iostream>

bool isWSL() { return false; }

int main() {
	Camera camera;
	ThreadPool pool(4);
	Chrono chrono;
	std::atomic_bool running(true);
	std::mutex drawMutex;
	std::queue<DisplayData*> solids, transparent;
	ChunkLoader loader(42, camera, pool, chrono, &running, drawMutex, solids, transparent);
	NoiseGenerator terrain(42);
	CaveGenerator caves(42);
	size_t checked = 0, crossings = 0;
	auto start = std::chrono::steady_clock::now();
	// Adjacent actual terrain columns, including negative chunk coordinates.
	for (int cz = -1; cz <= 1; ++cz) for (int cx = -1; cx <= 1; ++cx) {
		ivec2 pos(cx, cz);
		auto* map = terrain.getPerlinMap(pos);
		Chunk chunk(pos, map, caves, loader, pool, 1);
		chunk.loadBlocks(); // Production worker pool: height, then surface pass.
		Chunk repeated(pos, map, caves, loader, pool, 1);
		repeated.loadBlocks();
		for (int z = 0; z < 32; ++z) for (int x = 0; x < 32; ++x) {
			int height = map->heightMap[z * 32 + x];
			auto column = caves.prepareColumn(cx * 32 + x, cz * 32 + z, height);
			assert(chunk.getSubChunk(0)->getBlock({x,0,z}) == BEDROCK);
			for (int y = 1; y <= height - 7; ++y) {
				char block = chunk.getSubChunk(y/32)->getBlock({x,y%32,z});
				bool air = caves.classify(column,y) != CaveKind::Solid;
				assert(block == (air ? AIR : STONE));
				assert(block == repeated.getSubChunk(y/32)->getBlock({x,y%32,z}));
				++checked;
				if (air && x == 0) {
					int nx = cx * 32 - 1, nz = cz * 32 + z;
					if (caves.isAir(nx,y,nz,int(terrain.getHeight({nx,nz})))) ++crossings;
				}
			}
		}
	}
	assert(checked > 100000 && crossings > 0);
	// Exercise entrance preservation in the biome pass, including desert caps,
	// using a broad but deterministic entrance mask to guarantee coverage.
	CaveGenerationSettings settings;
	settings.entranceThreshold = -1;
	CaveGenerator mouths(42, settings);
	size_t openings = 0;
	for (Biome biome : {PLAINS, DESERT, MOUNTAINS, SNOWY, FOREST}) {
		PerlinMap map;
		map.heightMap = new double[32*32];
		map.treeMap = new double[32*32]();
		map.biomeMap = new Biome[32*32];
		std::fill_n(map.heightMap,32*32,150.0);
		std::fill_n(map.biomeMap,32*32,biome);
		Chunk chunk({0,0}, &map, mouths, loader, pool, 1);
		SubChunk sub({0,4,0}, &map, mouths, chunk, loader, 1);
		sub.loadHeight(0);
		sub.loadBiome(0);
		for (int z=0; z<32; ++z) for (int x=0; x<32; ++x)
		for (int y=145; y<=150; ++y) {
			if (mouths.isAir(x,y,z,150)) {
				assert(sub.getBlock({x,y-128,z}) == AIR);
				if (y==150) ++openings;
			}
		}
	}
	assert(openings > 0);
	std::cout << "Production chunks: " << checked << " underground stone/air checks, "
		<< crossings << " border crossings, " << openings << " preserved biome mouths; "
		<< std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count() << " seconds\n";
}
