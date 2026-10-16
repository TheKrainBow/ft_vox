#include "CaveWaterGenerator.hpp"
#include "LavaGenerator.hpp"
#include <algorithm>
#include <cassert>
#include <iostream>
#include <set>
#include <tuple>

using Positions=std::set<std::tuple<int,int,int>>;
static int floorDiv(int a,int b) { return a/b-(a%b<0); }
static int local(int a,int size) { return a-floorDiv(a,size)*size; }
static Positions positions(const std::vector<CaveWaterPos>& plan) {
	Positions out;
	for (auto p:plan) out.emplace(p.x,p.y,p.z);
	return out;
}
static std::vector<size_t> components(Positions remaining) {
	std::vector<size_t> sizes;
	while (!remaining.empty()) {
		auto [x,y,z]=*remaining.begin();
		std::vector<CaveWaterPos> queue{{x,y,z}};
		remaining.erase(remaining.begin());
		for (size_t i=0;i<queue.size();++i) for (auto d:std::array<CaveWaterPos,6>{{{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}}}) {
			auto p=queue[i];
			auto it=remaining.find({p.x+d.x,p.y+d.y,p.z+d.z});
			if (it!=remaining.end()) { queue.push_back({p.x+d.x,p.y+d.y,p.z+d.z});remaining.erase(it); }
		}
		sizes.push_back(queue.size());
	}
	return sizes;
}
static CaveWaterVoxel variedBasins(CaveWaterPos p) {
	bool big=local(floorDiv(p.x,48)+floorDiv(p.z,48),2)==0;
	int radius=big ? 12 : 2;
	bool pit=std::abs(local(p.x,48)-24)<=radius && std::abs(local(p.z,48)-24)<=radius;
	int y=local(p.y,48);
	return y>=20 || (pit && y>=16) ? CaveWaterVoxel::Air : CaveWaterVoxel::Solid;
}
int main() {
	assert(CaveWaterSettings{}.caveWaterFrequency==1.4f);
	CaveWaterPos min{-384,12,-384},max{384,64,384};
	for (bool springs:{false,true}) {
		CaveWaterSettings s;
		s.caveWaterFrequency=1; s.caveWaterMaxY=64; s.caveWaterMaxRadius=6;
		s.largePoolChance=0; s.caveWaterPoolChance=springs ? 0 : 1;
		s.caveWaterPocketChance=0; s.caveWaterSpringChance=springs ? 1 : 0;
		auto geometry=[&](CaveWaterPos p) {
			int x=local(p.x,16),z=local(p.z,16);
			if (springs) return p.y>16 && x<8 ? CaveWaterVoxel::Air : CaveWaterVoxel::Solid;
			bool pit=x>=6 && x<=10 && z>=6 && z<=10;
			return p.y>=20 || (pit && p.y>=18) ? CaveWaterVoxel::Air : CaveWaterVoxel::Solid;
		};
		auto before=positions(CaveWaterGenerator(42,s).generate(min,max,geometry));
		s.caveWaterFrequency=1.4f;
		auto after=positions(CaveWaterGenerator(42,s).generate(min,max,geometry));
		assert(std::includes(after.begin(),after.end(),before.begin(),before.end()));
		size_t n=components(before).size(),m=components(after).size();
		assert(n>50 && m*10>n*12 && m*10<n*17);
		std::cout << (springs ? "Springs" : "Pools") << ": " << n << " -> " << m << " features with +40% attempts\n";
	}
	// Small and large natural basins coexist; the large ones cannot be filled
	// by the old maximum radius. Their entire wet shoreline remains contained.
	CaveWaterSettings pools;
	pools.caveWaterFrequency=4; pools.caveWaterMaxY=64;
	pools.caveWaterPoolChance=1; pools.caveWaterPocketChance=0; pools.caveWaterSpringChance=0;
	auto wet=positions(CaveWaterGenerator(42,pools).generate(min,max,variedBasins));
	auto sizes=components(wet);
	assert(std::any_of(sizes.begin(),sizes.end(),[](size_t n) { return n>=400; }));
	assert(std::any_of(sizes.begin(),sizes.end(),[](size_t n) { return n<=100; }));
	for (auto [x,y,z]:wet) for (auto d:std::array<CaveWaterPos,5>{{{1,0,0},{-1,0,0},{0,-1,0},{0,0,1},{0,0,-1}}}) {
		CaveWaterPos q{x+d.x,y+d.y,z+d.z};
		if (variedBasins(q)==CaveWaterVoxel::Air) assert(wet.count({q.x,q.y,q.z}));
	}
	CaveWaterSettings dry; dry.caveWaterFrequency=0;
	CaveWaterGenerator noWater(42,dry);
	LavaSettings uniform;
	uniform.caveFrequency=4; uniform.caveMaxY=64; uniform.surfaceChance=0;
	uniform.deepFrequencyMultiplier=uniform.upperFrequencyMultiplier=1; uniform.springChance=0;
	auto hot=components(positions(LavaGenerator(42,uniform).caves(min,max,variedBasins,noWater)));
	assert(std::any_of(hot.begin(),hot.end(),[](size_t n) { return n>=400; }));
	assert(std::any_of(hot.begin(),hot.end(),[](size_t n) { return n<=100; }));
	std::cout << "Largest water/lava pools: " << *std::max_element(sizes.begin(),sizes.end())
		<< '/' << *std::max_element(hot.begin(),hot.end()) << " source blocks; small pools retained\n";
	// Identical cave geometry repeated at different elevations isolates the
	// depth bias from changes in the terrain/cave shape.
	LavaSettings depth;
	depth.surfaceChance=0;
	auto plan=LavaGenerator(42,depth).caves({min.x,4,min.z},{max.x,180,max.z},variedBasins,noWater);
	std::set<std::pair<int,int>> bottom,top;
	for (auto p:plan) {
		if (p.y<48) bottom.emplace(floorDiv(p.x,48),floorDiv(p.z,48));
		if (p.y>=144) top.emplace(floorDiv(p.x,48),floorDiv(p.z,48));
	}
	std::cout << "Lava regions near bedrock/upper caves: " << bottom.size() << '/' << top.size() << '\n';
	assert(bottom.size()>=64 && !top.empty() && bottom.size()>4*top.size());
}
