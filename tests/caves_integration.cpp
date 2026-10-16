#include "ChunkLoader.hpp"
#include "SubChunk.hpp"
#include <cassert>
#include <chrono>
#include <iostream>
#include <set>
#include <tuple>

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
	CaveGenerator& caves = loader.getCaveGenerator();
	CaveWaterSettings drySettings;
	drySettings.caveWaterFrequency = 0;
	LavaSettings noLava; noLava.caveFrequency=0; noLava.surfaceChance=0;
	ChunkLoader dryLoader(42, camera, pool, chrono, &running, drawMutex, solids, transparent, drySettings,noLava);
	size_t checked = 0, crossings = 0, waterVoxels = 0, wetChunks = 0, lavaVoxels = 0;
	std::set<std::tuple<int,int,int>> waterPositions, lavaPositions;
	auto start = std::chrono::steady_clock::now();
	// Adjacent actual terrain columns, including negative chunk coordinates.
	for (int cz = -4; cz <= 4; ++cz) for (int cx = -4; cx <= 4; ++cx) {
		ivec2 pos(cx, cz);
		size_t waterBefore = waterVoxels;
		auto* map = terrain.getPerlinMap(pos);
		Chunk chunk(pos, map, caves, loader, pool, 1);
		chunk.loadBlocks(); // Production worker pool: height, then surface pass.
		Chunk repeated(pos, map, caves, loader, pool, 1);
		repeated.loadBlocks();
		std::map<std::tuple<int,int,int>,char> surfacePlan;
		auto heightAt=[&](int x,int z) { return int(terrain.getHeight({x,z})); };
		auto baseGeometry=[&](CaveWaterPos p) {
			int h=heightAt(p.x,p.z);
			return p.y>h || caves.isAir(p.x,p.y,p.z,h) ? CaveWaterVoxel::Air : CaveWaterVoxel::Solid;
		};
		auto allowed=[&](int x,int z) { auto b=terrain.getBiome({x,z},heightAt(x,z)); return b==PLAINS || b==DESERT || b==MOUNTAINS; };
		for (auto p:loader.getLavaGenerator().surface({cx*32,0,cz*32},{cx*32+32,640,cz*32+32},heightAt,baseGeometry,allowed))
			surfacePlan[{p.position.x,p.position.y,p.position.z}]=p.block;
		Chunk dry(pos, map, caves, dryLoader, pool, 1);
		dry.loadBlocks();
		for (int z = 0; z < 32; ++z) for (int x = 0; x < 32; ++x) {
			int height = map->heightMap[z * 32 + x];
			auto column = caves.prepareColumn(cx * 32 + x, cz * 32 + z, height);
			assert(chunk.getSubChunk(0)->getBlock({x,0,z}) == BEDROCK);
			for (int y = 1; y <= height - 7; ++y) {
				char block = chunk.getSubChunk(y/32)->getBlock({x,y%32,z});
				bool air = caves.classify(column,y) != CaveKind::Solid;
				char dryBlock = dry.getSubChunk(y/32)->getBlock({x,y%32,z});
				assert(dryBlock == (air ? AIR : STONE));
				if (block == WATER) {
					++waterVoxels;
					waterPositions.emplace(cx*32+x,y,cz*32+z);
					assert(y >= 12 && y < 180 && y < height-24);
				} else if (block==LAVA) {
					++lavaVoxels;
					lavaPositions.emplace(cx*32+x,y,cz*32+z);
					assert(y>=8 && y<180 && y<height-24);
				} else assert(block == dryBlock);
				assert(block == repeated.getSubChunk(y/32)->getBlock({x,y%32,z}));
				++checked;
				if (air && x == 0) {
					int nx = cx * 32 - 1, nz = cz * 32 + z;
					if (caves.isAir(nx,y,nz,int(terrain.getHeight({nx,nz})))) ++crossings;
				}
			}
			// Every surface block and ocean source must match the disabled baseline.
			for (int y=std::max(1,height-24); y<=std::max(height, OCEAN_HEIGHT)+1; ++y) {
				auto* wetSub = chunk.getSubChunk(y/32);
				auto* drySub = dry.getSubChunk(y/32);
				assert(wetSub && drySub);
				auto planned=surfacePlan.find({cx*32+x,y,cz*32+z});
				char expected=planned==surfacePlan.end() ? drySub->getBlock({x,y%32,z}) : planned->second;
				assert(wetSub->getBlock({x,y%32,z}) == expected);
			}
		}
		wetChunks += waterVoxels > waterBefore;
	}
	assert(wetChunks > 0 && wetChunks < 81/2);
	assert(checked > 100000 && crossings > 0 && waterVoxels > 0);
	// Pools/pockets remain contained. Only a recessed single-source spring may
	// have one open horizontal outlet, with a nearby landing floor below it.
	size_t springs = 0;
	for (auto [x,y,z] : waterPositions) {
		int exits = 0;
		glm::ivec3 mouth{};
		for (auto d : {glm::ivec3(1,0,0),glm::ivec3(-1,0,0),glm::ivec3(0,0,1),glm::ivec3(0,0,-1),glm::ivec3(0,-1,0)}) {
			int nx=x+d.x, ny=y+d.y, nz=z+d.z;
			if (nx < -128 || nx >= 160 || nz < -128 || nz >= 160) continue;
			if (caves.isAir(nx,ny,nz,int(terrain.getHeight({nx,nz}))) && !waterPositions.count({nx,ny,nz})) {
				assert(d.y == 0);
				++exits;
				mouth = {nx,ny,nz};
			}
		}
		if (!exits) continue;
		++springs;
		assert(exits == 1);
		int height = int(terrain.getHeight({x,z}));
		assert(!caves.isAir(x,y,z,height) && !caves.isAir(x,y+1,z,height));
		int mouthHeight = int(terrain.getHeight({mouth.x,mouth.z}));
		int drop = 1;
		while (drop <= 16 && caves.isAir(mouth.x,mouth.y-drop,mouth.z,mouthHeight)) ++drop;
		assert(drop >= 2 && drop <= 16);
	}
	assert(springs > 0);
	assert(lavaVoxels>0);
	for (auto [x,y,z]:lavaPositions) for (int dz=-2;dz<=2;++dz) for (int dy=-2;dy<=2;++dy) for (int dx=-2;dx<=2;++dx)
		assert(!waterPositions.count({x+dx,y+dy,z+dz}));
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
	// Verify the rare surface planner also finds valid pools on actual terrain,
	// rather than only on the flat synthetic fixture.
	std::map<std::pair<int,int>,CaveGenerator::Column> surfaceColumns;
	auto surfaceHeight=[&](int x,int z) {
		auto key=std::make_pair(x,z);
		auto it=surfaceColumns.find(key);
		if (it==surfaceColumns.end()) it=surfaceColumns.emplace(key,caves.prepareColumn(x,z,int(terrain.getHeight({x,z})))).first;
		return it->second.surfaceY;
	};
	auto surfaceGeometry=[&](CaveWaterPos p) {
		int h=surfaceHeight(p.x,p.z);
		return p.y>h || caves.classify(surfaceColumns.at({p.x,p.z}),p.y)!=CaveKind::Solid ? CaveWaterVoxel::Air : CaveWaterVoxel::Solid;
	};
	auto surfaceAllowed=[&](int x,int z) { auto b=terrain.getBiome({x,z},surfaceHeight(x,z)); return b==PLAINS || b==DESERT || b==MOUNTAINS; };
	auto surfacePools=loader.getLavaGenerator().surface({-8192,0,-8192},{8192,640,8192},surfaceHeight,surfaceGeometry,surfaceAllowed);
	size_t realSurfaceLava=0;
	for (auto p:surfacePools) if (p.block==LAVA) ++realSurfaceLava;
	assert(realSurfaceLava>0);
	std::cout << "Production chunks: " << checked << " underground stone/air checks, "
		<< realSurfaceLava << " planned rare surface lava voxels, " << lavaVoxels << " cave lava voxels, " << waterVoxels << " cave water voxels in " << wetChunks << "/81 chunks, " << springs << " wall springs, " << crossings << " border crossings, " << openings << " preserved biome mouths; "
		<< std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count() << " seconds\n";
}
