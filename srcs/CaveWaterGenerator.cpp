#include "CaveWaterGenerator.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <unordered_set>
#ifdef DEBUG_CAVE_WATER
#include <iostream>
#endif

namespace {
uint64_t mix(uint64_t v) {
	v += 0x9e3779b97f4a7c15ULL;
	v = (v ^ (v >> 30)) * 0xbf58476d1ce4e5b9ULL;
	v = (v ^ (v >> 27)) * 0x94d049bb133111ebULL;
	return v ^ (v >> 31);
}
struct Hash {
	size_t operator()(CaveWaterPos p) const {
		return mix(uint32_t(p.x)) ^ mix(uint64_t(uint32_t(p.y)) + 17) ^ mix(uint64_t(uint32_t(p.z)) + 37);
	}
};
int floorDiv(int a, int b) { return a / b - (a % b < 0); }
constexpr std::array<CaveWaterPos,6> sides = {{{1,0,0},{-1,0,0},{0,0,1},{0,0,-1},{0,-1,0},{0,1,0}}};
CaveWaterPos add(CaveWaterPos p, CaveWaterPos d) { return {p.x+d.x,p.y+d.y,p.z+d.z}; }

// Reject a leaking basin rather than clipping water at an artificial boundary.
std::vector<CaveWaterPos> pool(CaveWaterPos floor, int radius, int depth, int budget,
	const CaveWaterGenerator::Sample& sample) {
	int level = floor.y + depth - 1;
	std::vector<CaveWaterPos> queue;
	queue.reserve(std::min(budget, 256));
	std::unordered_set<CaveWaterPos,Hash> visited;
	queue.push_back(floor);
	visited.insert(floor);
	for (size_t i = 0; i < queue.size(); ++i) {
		for (auto d : sides) {
			auto p = add(queue[i], d);
			if (p.y > level) continue;
			auto voxel = sample(p);
			if (voxel == CaveWaterVoxel::Protected) return {};
			if (voxel == CaveWaterVoxel::Solid) continue;
			if (std::abs(p.x-floor.x) > radius || std::abs(p.z-floor.z) > radius ||
				p.y < floor.y || p.y > floor.y+3) return {};
			if (visited.insert(p).second) {
				if (int(queue.size()) >= budget) return {};
				queue.push_back(p);
			}
		}
	}
	// A dry opening directly above the water surface makes this a cave pool.
	bool exposed = false;
	for (auto p : queue) if (p.y == level) {
		auto above = sample({p.x,p.y+1,p.z});
		if (above == CaveWaterVoxel::Protected) return {};
		exposed |= above == CaveWaterVoxel::Air;
	}
	return exposed && queue.size() >= 3 ? queue : std::vector<CaveWaterPos>{};
}

// Carve only a tiny water-filled cavity. Every wet face must remain sealed;
// the existing cave may expose its dry rim from above, never a draining side.
std::vector<CaveWaterPos> pocket(CaveWaterPos air, int radius, int depth, uint64_t hash,
	const CaveWaterGenerator::Sample& sample) {
	for (int dir = 0; dir < 4; ++dir) {
		auto d = sides[(dir + int(hash % 4)) % 4];
		if (sample(add(air,d)) != CaveWaterVoxel::Solid) continue;
		CaveWaterPos center = {air.x + d.x*(radius-1), air.y-1, air.z + d.z*(radius-1)};
		std::vector<CaveWaterPos> water;
		std::unordered_set<CaveWaterPos,Hash> shape;
		for (int z=-radius; z<=radius; ++z) for (int x=-radius; x<=radius; ++x)
		for (int y=0; y<depth; ++y) {
			float dx = x + 0.22f*std::sin(z*1.7f + float(hash%31));
			float dz = z + 0.22f*std::sin(x*1.3f + float(hash%23));
			float taper = 1.0f - 0.25f * float(y) / depth;
			if ((dx*dx + dz*dz) > radius*radius*taper) continue;
			auto p = CaveWaterPos{center.x+x, center.y-y, center.z+z};
			shape.insert(p);
			water.push_back(p);
		}
		bool valid = !water.empty(), nearCave = false;
		for (auto p : water) {
			if (sample(p) != CaveWaterVoxel::Solid) { valid = false; break; }
			for (auto step : sides) {
				auto q = add(p, step);
				if (shape.count(q)) continue;
				auto v = sample(q);
				if (v == CaveWaterVoxel::Protected) valid = false;
				if (v == CaveWaterVoxel::Air) {
					if (q.y <= center.y) valid = false;
					else nearCave = true;
				}
			}
		}
		if (valid && nearCave) return water;
	}
	return {};
}

// One source recessed into a wall, with one open face toward the cave.
// Require a nearby cave floor below the outlet rather than a bottomless shaft.
std::vector<CaveWaterPos> spring(CaveWaterPos mouth, uint64_t hash,
	const CaveWaterGenerator::Sample& sample) {
	int drop = 0;
	for (int depth = 1; depth <= 16; ++depth) {
		auto voxel = sample({mouth.x,mouth.y-depth,mouth.z});
		if (voxel == CaveWaterVoxel::Protected) return {};
		if (voxel == CaveWaterVoxel::Solid) { drop = depth; break; }
	}
	if (drop < 2) return {};
	for (int dir = 0; dir < 4; ++dir) {
		auto d = sides[(dir + int(hash % 4)) % 4];
		auto source = add(mouth,d);
		if (sample(source) != CaveWaterVoxel::Solid) continue;
		bool valid = true;
		for (auto step : sides) {
			auto q = add(source,step);
			if (q == mouth) continue;
			if (sample(q) != CaveWaterVoxel::Solid) { valid = false; break; }
		}
		if (valid) return {source};
	}
	return {};
}
}

CaveWaterGenerator::CaveWaterGenerator(uint64_t seed, CaveWaterSettings settings)
	: m_seed(seed), m_settings(settings) {
	m_settings.regionSize = std::clamp(settings.regionSize, 32, 256);
	m_settings.caveWaterMinRadius = std::clamp(settings.caveWaterMinRadius, 2, 12);
	m_settings.caveWaterMaxRadius = std::clamp(settings.caveWaterMaxRadius, m_settings.caveWaterMinRadius, 12);
	m_settings.regionSize = std::max(m_settings.regionSize, 2*(6+m_settings.caveWaterMaxRadius)+1);
	m_settings.caveWaterFrequency = std::clamp(settings.caveWaterFrequency, 0.0f, 1.0f);
	m_settings.caveWaterMinY = std::max(2, settings.caveWaterMinY);
	m_settings.minimumUndergroundDepth = std::max(16, settings.minimumUndergroundDepth);
	m_settings.maxVisitedVoxels = std::clamp(settings.maxVisitedVoxels, 16, 4096);
	m_settings.caveWaterPoolChance = std::max(0.0f, settings.caveWaterPoolChance);
	m_settings.caveWaterPocketChance = std::max(0.0f, settings.caveWaterPocketChance);
	m_settings.caveWaterSpringChance = std::max(0.0f, settings.caveWaterSpringChance);
}

std::vector<CaveWaterPos> CaveWaterGenerator::generate(CaveWaterPos min, CaveWaterPos max, const Sample& geometry, bool cacheFeatures) const {
	const auto& s = m_settings;
	std::vector<CaveWaterPos> result;
	float totalWeight = s.caveWaterPoolChance + s.caveWaterPocketChance + s.caveWaterSpringChance;
	if (s.caveWaterFrequency <= 0 || max.y <= s.caveWaterMinY || min.y >= s.caveWaterMaxY ||
		totalWeight <= 0) return result;
	// Search can move a feature six blocks horizontally and eight vertically.
	int halo = 6 + s.caveWaterMaxRadius * 2;
	auto sample = [&](CaveWaterPos p) {
		return p.y < s.caveWaterMinY || p.y >= s.caveWaterMaxY
			? CaveWaterVoxel::Protected : geometry(p);
	};
	for (int rz = floorDiv(min.z-halo,s.regionSize); rz <= floorDiv(max.z-1+halo,s.regionSize); ++rz)
	for (int rx = floorDiv(min.x-halo,s.regionSize); rx <= floorDiv(max.x-1+halo,s.regionSize); ++rx)
	for (int ry = floorDiv(std::max(min.y-12,s.caveWaterMinY-8),s.regionSize);
		ry <= floorDiv(std::min(max.y+8,s.caveWaterMaxY+8),s.regionSize); ++ry) {
		uint64_t h = mix(m_seed ^ mix(uint32_t(rx)) ^ mix(uint64_t(uint32_t(ry))+71) ^ mix(uint64_t(uint32_t(rz))+139));
		auto random = [&](int salt) { return (mix(h+salt)>>11) * (1.0/9007199254740992.0); };
		if (random(0) >= s.caveWaterFrequency) continue;
		// Keep wet volumes inside their feature region so neighboring features
		// cannot remove each other's containment walls. Chunk seams are unrelated.
		int inset = 6+s.caveWaterMaxRadius;
		CaveWaterPos center{rx*s.regionSize+inset+int(random(1)*(s.regionSize-2*inset)),
			ry*s.regionSize+12+int(random(2)*(s.regionSize-24)),
			rz*s.regionSize+inset+int(random(3)*(s.regionSize-2*inset))};
		if (center.x+halo < min.x || center.x-halo >= max.x ||
			center.z+halo < min.z || center.z-halo >= max.z ||
			center.y+12 < min.y || center.y-12 >= max.y) continue;
		// The decision and all search limits are independent of the output box.
		double type = random(4)*totalWeight;
		bool isSpring = type >= s.caveWaterPoolChance+s.caveWaterPocketChance;
		bool isPocket = !isSpring && type >= s.caveWaterPoolChance;
		int radius = s.caveWaterMinRadius;
		// 85% of pools use radii 2..5; the remaining ponds may reach the maximum.
		int limit = random(5) < 0.85 ? std::min(s.caveWaterMaxRadius, std::max(radius,5)) : s.caveWaterMaxRadius;
		radius += int(random(6)*(limit-radius+1));
		auto analyse = [&]() {
			std::vector<CaveWaterPos> water;
			for (int dz=-6; dz<=6 && water.empty(); dz+=2)
			for (int dx=-6; dx<=6 && water.empty(); dx+=2)
			for (int dy=-8; dy<=8 && water.empty(); ++dy) {
				CaveWaterPos p{center.x+dx,center.y+dy,center.z+dz};
				if (sample(p) != CaveWaterVoxel::Air) continue;
				if (isSpring) water = spring(p,h,sample);
				else if (isPocket) {
					water = pocket(p,std::min(radius,3),1+int(random(7)*4),h,sample);
					if (int(water.size()) > s.maxVisitedVoxels) water.clear();
				}
				else if (sample({p.x,p.y-1,p.z}) == CaveWaterVoxel::Solid) {
					for (int depth=1+int(random(7)*4); depth>=1 && water.empty(); --depth)
						water = pool(p,radius,depth,s.maxVisitedVoxels,sample);
				}
			}
	#ifdef DEBUG_CAVE_WATER
			int waterLevel = water.empty() ? -1 : std::max_element(water.begin(),water.end(),
				[](CaveWaterPos a, CaveWaterPos b) { return a.y < b.y; })->y;
			std::clog << "cave water " << center.x << ',' << center.y << ',' << center.z
				<< (isSpring ? " spring" : isPocket ? " pocket" : " pool") << " radius=" << (isSpring ? 1 : radius)
				<< " level=" << waterLevel << " voxels=" << water.size() << (water.empty() ? " rejected" : " accepted") << '\n';
	#endif
			return water;
		};
		std::vector<CaveWaterPos> water;
		if (!cacheFeatures) water = analyse();
		else {
			std::shared_future<std::vector<CaveWaterPos>> future;
			std::shared_ptr<std::promise<std::vector<CaveWaterPos>>> producer;
			{
				std::lock_guard<std::mutex> lock(m_cacheMutex);
				auto key = std::array<int,3>{rx,ry,rz};
				auto found = m_features.find(key);
				if (found != m_features.end()) future = found->second;
				else {
					producer = std::make_shared<std::promise<std::vector<CaveWaterPos>>>();
					future = producer->get_future().share();
					// Bound retained plans during long journeys. Copies of in-flight
					// futures remain valid if their cache entry is evicted.
					if (m_features.size() >= 2048) m_features.erase(m_features.begin());
					m_features.emplace(key,future);
				}
			}
			if (producer) {
				try { producer->set_value(analyse()); }
				catch (...) { producer->set_exception(std::current_exception()); throw; }
			}
			water = future.get();
		}
		for (auto p : water)
			if (p.x>=min.x && p.x<max.x && p.y>=min.y && p.y<max.y && p.z>=min.z && p.z<max.z)
				result.push_back(p);
	}
	return result;
}
