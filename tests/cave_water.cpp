#include "CaveWaterGenerator.hpp"
#include "WaterSimulation.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
#include <set>
#include <tuple>

using Positions = std::set<std::tuple<int,int,int>>;
static Positions positions(const std::vector<CaveWaterPos>& water) {
	Positions out;
	for (auto p : water) out.emplace(p.x,p.y,p.z);
	return out;
}
static int local(int x) { return (x%16+16)%16; }
static CaveWaterVoxel basins(CaveWaterPos p) {
	int x=local(p.x), z=local(p.z);
	bool pit = x>=6 && x<=10 && z>=6 && z<=10;
	return p.y >= 20 || (pit && p.y>=18) ? CaveWaterVoxel::Air : CaveWaterVoxel::Solid;
}
int main() {
	CaveWaterSettings settings;
	settings.caveWaterFrequency = 1;
	settings.caveWaterMaxY = 64;
	settings.caveWaterPoolChance = 1;
	settings.caveWaterPocketChance = 0;
	settings.caveWaterSpringChance = 0;
	CaveWaterGenerator gen(42,settings), repeat(42,settings), other(43,settings);
	CaveWaterPos min{-128,12,-128}, max{128,64,128};
	auto all = positions(gen.generate(min,max,basins));
	assert(!all.empty());
	assert(all == positions(repeat.generate(min,max,basins)));
	assert(all != positions(other.generate(min,max,basins)));
	for (auto [x,y,z] : all) {
		assert(y>=18 && y<=19);
		assert(basins({x,y,z}) == CaveWaterVoxel::Air);
	}
	// Splitting output boxes (also vertically) must not change any feature.
	Positions tiled;
	for (int z=96; z>=-128; z-=32) for (int x=96; x>=-128; x-=32)
	for (int y=12; y<64; y+=16) {
		auto part = positions(gen.generate({x,y,z},{x+32,std::min(y+16,64),z+32},basins));
		tiled.insert(part.begin(),part.end());
	}
	assert(all == tiled);
	auto flat = [](CaveWaterPos p) { return p.y>=18 ? CaveWaterVoxel::Air : CaveWaterVoxel::Solid; };
	assert(gen.generate(min,max,flat).empty()); // Unbounded cave floor leaks.
	auto unknown = [](CaveWaterPos) { return CaveWaterVoxel::Protected; };
	assert(gen.generate(min,max,unknown).empty());
	assert(gen.generate({-128,64,-128},{128,96,128},basins).empty());
	settings.caveWaterFrequency = 0;
	assert(CaveWaterGenerator(42,settings).generate(min,max,basins).empty());
	settings.caveWaterFrequency = 1;
	settings.caveWaterPoolChance = 0;
	settings.caveWaterPocketChance = 1;
	CaveWaterGenerator pockets(42,settings);
	// Cave wall above a solid floor, with a dry rim exposing the pocket.
	auto walls = [](CaveWaterPos p) {
		return p.y>=20 && local(p.x)<8 ? CaveWaterVoxel::Air : CaveWaterVoxel::Solid;
	};
	auto pocketWater = pockets.generate(min,max,walls);
	assert(!pocketWater.empty());
	for (auto p : pocketWater) {
		assert(p.y>=16 && p.y<20);
		assert(walls(p) == CaveWaterVoxel::Solid);
	}
	// A cave extending down alongside the wall would drain a pocket: reject it.
	auto openWall = [](CaveWaterPos p) { return local(p.x)<8 ? CaveWaterVoxel::Air : CaveWaterVoxel::Solid; };
	assert(pockets.generate(min,max,openWall).empty());
	settings.caveWaterPocketChance = 0;
	settings.caveWaterSpringChance = 1;
	CaveWaterGenerator springs(42,settings);
	auto springWalls = [](CaveWaterPos p) {
		return p.y>16 && local(p.x)<8 ? CaveWaterVoxel::Air : CaveWaterVoxel::Solid;
	};
	auto springWater = springs.generate(min,max,springWalls);
	assert(!springWater.empty());
	std::set<std::tuple<int,int,int>> regions;
	for (auto p : springWater) {
		assert(springWalls(p) == CaveWaterVoxel::Solid);
		assert(p.y>=18 && p.y<=32);
		assert(regions.emplace(int(std::floor(p.x/48.0)),int(std::floor(p.y/48.0)),
			int(std::floor(p.z/48.0))).second); // Exactly one source per feature.
	}
	Positions springTiles;
	for (int z=96; z>=-128; z-=32) for (int x=96; x>=-128; x-=32)
	for (int y=12; y<64; y+=16) {
		auto part = positions(springs.generate({x,y,z},{x+32,std::min(y+16,64),z+32},springWalls));
		springTiles.insert(part.begin(),part.end());
	}
	assert(springTiles == positions(springWater));
	assert(springs.generate(min,max,openWall).empty()); // No local landing floor.
	assert(springs.generate(min,max,unknown).empty());
	assert(springs.generate(min,max,flat).empty()); // No wall to recess the source into.
	// Feed a generated hole through the production solver: it spills sideways,
	// falls to the floor and spreads there while retaining just one source.
	auto source = springWater.front();
	std::map<WaterPos,char> flowing;
	WaterPos sourcePos{source.x,source.y,source.z};
	flowing[sourcePos] = WATER;
	auto read = [&](WaterPos p) {
		auto it = flowing.find(p);
		return it != flowing.end() ? it->second :
			springWalls({p[0],p[1],p[2]}) == CaveWaterVoxel::Air ? char(AIR) : char(STONE);
	};
	auto write = [&](WaterPos p, char b) { flowing[p] = b; };
	processWaterTick(read,write,sourcePos);
	WaterPos mouth{source.x-1,source.y,source.z};
	assert(read(mouth) == WATER_FLOW_1);
	for (int y=source.y; y>17; --y) {
		processWaterTick(read,write,{mouth[0],y,mouth[2]});
		assert(read({mouth[0],y-1,mouth[2]}) == WATER_FALLING);
	}
	processWaterTick(read,write,{mouth[0],17,mouth[2]});
	assert(isWater(read({mouth[0],17,mouth[2]+1})));
	int sources = 0;
	for (auto entry : flowing) sources += entry.second == WATER;
	assert(sources == 1 && read(sourcePos) == WATER);
	std::cout << "Cave water: " << all.size() << " basin voxels, " << pocketWater.size()
		<< " pocket voxels, " << springWater.size()
		<< " wall springs; borders, containment, protection and spring flow passed\n";
}
