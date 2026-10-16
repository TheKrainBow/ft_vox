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
using Hash = CaveWaterPosHash;
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
				p.y < floor.y) return {};
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
std::vector<CaveWaterPos> spring(CaveWaterPos mouth, uint64_t hash, int clearance,
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
		if (valid && clearance > 0) {
			// Test the possible fall/spread against protected geometry, including
			// the other fluid's reservations, before committing the source.
			for (int y = mouth.y-drop+1; y <= mouth.y && valid; ++y)
			for (int z = -clearance; z <= clearance && valid; ++z)
			for (int x = -clearance; x <= clearance; ++x)
				if (std::abs(x)+std::abs(z) <= clearance &&
					sample({mouth.x+x,y,mouth.z+z}) == CaveWaterVoxel::Protected) { valid = false; break; }
		}
		if (valid) return {source};
	}
	return {};
}
}

CaveWaterGenerator::CaveWaterGenerator(uint64_t seed, CaveWaterSettings settings)
	: m_seed(seed), m_settings(settings) {
	m_settings.regionSize = std::clamp(settings.regionSize, 32, 256);
	m_settings.caveWaterMinRadius = std::clamp(settings.caveWaterMinRadius, 2, 24);
	m_settings.caveWaterMaxRadius = std::clamp(settings.caveWaterMaxRadius, m_settings.caveWaterMinRadius, 24);
	// A radius-three pocket reaches five blocks; retain a solid boundary too.
	m_settings.regionSize = std::max(m_settings.regionSize, 2*(6+std::max(6,m_settings.caveWaterMaxRadius))+1);
	m_settings.caveWaterFrequency = std::clamp(settings.caveWaterFrequency, 0.0f, 4.0f);
	m_settings.caveWaterMinY = std::max(2, settings.caveWaterMinY);
	m_settings.minimumUndergroundDepth = std::max(16, settings.minimumUndergroundDepth);
	m_settings.maxVisitedVoxels = std::clamp(settings.maxVisitedVoxels, 16, 16384);
	m_settings.largePoolChance = std::clamp(settings.largePoolChance, 0.0f, 1.0f);
	m_settings.smallPoolMaxRadius = std::clamp(settings.smallPoolMaxRadius, m_settings.caveWaterMinRadius, m_settings.caveWaterMaxRadius);
	m_settings.largePoolMinRadius = std::clamp(settings.largePoolMinRadius, m_settings.caveWaterMinRadius, m_settings.caveWaterMaxRadius);
	m_settings.maxDepth = std::clamp(settings.maxDepth, 1, 8);
	m_settings.deepFrequencyMultiplier = std::clamp(settings.deepFrequencyMultiplier, 0.0f, 4.0f);
	m_settings.upperFrequencyMultiplier = std::clamp(settings.upperFrequencyMultiplier, 0.0f, 4.0f);
	m_settings.springClearanceRadius = std::clamp(settings.springClearanceRadius, 0, 9);
	m_settings.caveWaterPoolChance = std::max(0.0f, settings.caveWaterPoolChance);
	m_settings.caveWaterPocketChance = std::max(0.0f, settings.caveWaterPocketChance);
	m_settings.caveWaterSpringChance = std::max(0.0f, settings.caveWaterSpringChance);
}

std::vector<CaveWaterPos> CaveWaterGenerator::generate(CaveWaterPos min, CaveWaterPos max, const Sample& geometry, bool cacheFeatures) const {
	const auto& s = m_settings;
	std::vector<CaveWaterPos> result;
	float totalWeight = s.caveWaterPoolChance + s.caveWaterPocketChance + s.caveWaterSpringChance;
	if (max.x <= min.x || max.z <= min.z || max.y <= min.y ||
		s.caveWaterFrequency <= 0 || max.y <= s.caveWaterMinY || min.y >= s.caveWaterMaxY ||
		totalWeight <= 0) return result;
	const int verticalHalo = 8+s.maxDepth;
	auto sample = [&](CaveWaterPos p) {
		return p.y < s.caveWaterMinY || p.y >= s.caveWaterMaxY
			? CaveWaterVoxel::Protected : geometry(p);
	};
	// The inset keeps every wet XZ coordinate inside its feature region.
	// Analyze complete regions, including all attempts, independently of output.
	for (int rz = floorDiv(min.z,s.regionSize); rz <= floorDiv(max.z-1,s.regionSize); ++rz)
	for (int rx = floorDiv(min.x,s.regionSize); rx <= floorDiv(max.x-1,s.regionSize); ++rx)
	for (int ry = floorDiv(std::max(min.y-verticalHalo,s.caveWaterMinY-8),s.regionSize);
		ry <= floorDiv(std::min(max.y+verticalHalo,s.caveWaterMaxY+8),s.regionSize); ++ry) {
		float t = std::clamp((ry*s.regionSize+s.regionSize*0.5f-s.caveWaterMinY)/
			std::max(1.0f,float(s.caveWaterMaxY-s.caveWaterMinY)),0.0f,1.0f);
		float multiplier = s.upperFrequencyMultiplier +
			(s.deepFrequencyMultiplier-s.upperFrequencyMultiplier)*(1-t)*(1-t);
		float frequency = std::clamp(s.caveWaterFrequency*multiplier,0.0f,4.0f);
		if (frequency <= 0) continue;
		uint64_t regionHash = mix(m_seed ^ mix(uint32_t(rx)) ^ mix(uint64_t(uint32_t(ry))+71) ^ mix(uint64_t(uint32_t(rz))+139));
		auto analyse = [&]() {
			std::vector<CaveWaterPos> plan;
			std::unordered_set<CaveWaterPos,Hash> reserved;
			auto featureSample = [&](CaveWaterPos p) {
				return reserved.count(p) ? CaveWaterVoxel::Protected : sample(p);
			};
			for (int attempt = 0; attempt < 4 && attempt < frequency; ++attempt) {
				uint64_t h = attempt == 0 ? regionHash : mix(regionHash+uint64_t(attempt)*0x9e3779b97f4a7c15ULL);
				auto random = [&](int salt) { return (mix(h+salt)>>11) * (1.0/9007199254740992.0); };
				if (random(0) >= std::min(1.0f,frequency-attempt)) continue;
				int budget = s.maxVisitedVoxels-int(plan.size());
				if (budget <= 0) break;
				int inset = 6+std::max(6,s.caveWaterMaxRadius);
				CaveWaterPos center{rx*s.regionSize+inset+int(random(1)*(s.regionSize-2*inset)),
					ry*s.regionSize+12+int(random(2)*(s.regionSize-24)),
					rz*s.regionSize+inset+int(random(3)*(s.regionSize-2*inset))};
				double type = random(4)*totalWeight;
				bool isSpring = type >= s.caveWaterPoolChance+s.caveWaterPocketChance;
				bool isPocket = !isSpring && type >= s.caveWaterPoolChance;
				bool large = random(5) < s.largePoolChance && s.caveWaterMaxRadius > s.smallPoolMaxRadius;
				int lowRadius = large ? s.largePoolMinRadius : s.caveWaterMinRadius;
				int highRadius = large ? s.caveWaterMaxRadius : s.smallPoolMaxRadius;
				int radius = lowRadius+int(random(6)*(highRadius-lowRadius+1));
				if (isPocket) {
					int limit = std::max(2,std::min(8,s.caveWaterMaxRadius/2));
					int low = std::min(5,limit);
					radius = large ? low+int(random(6)*(limit-low+1)) : std::min(radius,3);
				}
				int lowDepth = large ? std::min(3,s.maxDepth) : 1;
				int highDepth = large ? s.maxDepth : std::min(3,s.maxDepth);
				int depth = lowDepth+int(random(7)*(highDepth-lowDepth+1));
				std::vector<CaveWaterPos> water;
				for (int dz=-6; dz<=6 && water.empty(); dz+=2)
				for (int dx=-6; dx<=6 && water.empty(); dx+=2)
				for (int dy=-8; dy<=8 && water.empty(); ++dy) {
					CaveWaterPos p{center.x+dx,center.y+dy,center.z+dz};
					if (featureSample(p) != CaveWaterVoxel::Air) continue;
					if (isSpring) water = spring(p,h,s.springClearanceRadius,featureSample);
					else if (isPocket) water = pocket(p,radius,depth,h,featureSample);
					else if (featureSample({p.x,p.y-1,p.z}) == CaveWaterVoxel::Solid) {
						for (int fillDepth=depth; fillDepth>=1 && water.empty(); --fillDepth)
							water = pool(p,radius,fillDepth,budget,featureSample);
					}
					if (int(water.size()) > budget) water.clear();
					if (std::any_of(water.begin(),water.end(),[&](CaveWaterPos q) {
						return q.x < rx*s.regionSize || q.x >= (rx+1)*s.regionSize ||
							q.z < rz*s.regionSize || q.z >= (rz+1)*s.regionSize;
					})) water.clear();
					if (large && !isSpring && !water.empty()) {
						int minX=water.front().x,maxX=minX,minZ=water.front().z,maxZ=minZ;
						for (auto q:water) { minX=std::min(minX,q.x);maxX=std::max(maxX,q.x);minZ=std::min(minZ,q.z);maxZ=std::max(maxZ,q.z); }
						int span = isPocket ? 8 : std::max(8,2*s.largePoolMinRadius-2);
						if (std::max(maxX-minX+1,maxZ-minZ+1) < span) water.clear();
					}
				}
#ifdef DEBUG_CAVE_WATER
				std::clog << "cave fluid " << center.x << ',' << center.y << ',' << center.z
					<< " attempt=" << attempt << (isSpring ? " spring" : isPocket ? " pocket" : " pool")
					<< " radius=" << (isSpring ? 1 : radius) << " voxels=" << water.size() << '\n';
#endif
				plan.insert(plan.end(),water.begin(),water.end());
				// Later attempts must preserve the wet volume and every supporting
				// face of earlier features, including recessed spring walls.
				for (auto p:water) {
					reserved.insert(p);
					for (auto d:sides) reserved.insert(add(p,d));
				}
			}
			return plan;
		};
		std::vector<CaveWaterPos> uncached;
		std::shared_future<std::vector<CaveWaterPos>> future;
		const std::vector<CaveWaterPos>* water = &uncached;
		if (!cacheFeatures) uncached = analyse();
		else {
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
			water = &future.get();
		}
		for (auto p : *water)
			if (p.x>=min.x && p.x<max.x && p.y>=min.y && p.y<max.y && p.z>=min.z && p.z<max.z)
				result.push_back(p);
	}
	return result;
}
