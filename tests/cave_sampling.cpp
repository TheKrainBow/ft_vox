#include "CaveGenerator.hpp"
#include <array>
#include <cassert>
#include <future>
#include <iostream>
#include <optional>

static uint64_t fingerprint(const CaveGenerator& caves, bool reverse = false) {
	// XOR indexed values so visiting the same volume in either order agrees.
	uint64_t hash = 0;
	for (int i = 0; i < 64*32; ++i) {
		int j = reverse ? 64*32-1-i : i;
		int x = j%64-32, z = j/64-16;
		auto column = caves.prepareColumn(x,z,180);
		for (int k = 0; k <= 180; ++k) {
			int y = reverse ? 180-k : k;
			uint64_t value = uint64_t(j*181+y)*0x9e3779b185ebca87ULL;
			hash ^= (value ^ (value >> 29))*uint64_t(1+int(caves.classify(column,y)));
		}
	}
	return hash;
}

int main() {
	std::optional<CaveGenerator> reused;
	for (int step : {2,4}) for (unsigned seed : {42,1337,98765}) {
		CaveGenerationSettings settings;
		settings.noiseSampleStep = step;
		CaveGenerator exact(seed), sampled(seed,settings);
		size_t changed = 0, noodles = 0, retainedNoodles = 0;
		std::array<size_t,5> counts{};
		for (int z = -32; z < 32; ++z) for (int x = -64; x < 64; ++x) {
			auto column = exact.prepareColumn(x,z,160);
			auto coarseColumn = sampled.prepareColumn(x,z,160);
			for (int y = 0; y <= 160; ++y) {
				auto a = exact.classify(column,y), b = sampled.classify(coarseColumn,y);
				++counts[int(b)];
				changed += (a == CaveKind::Solid) != (b == CaveKind::Solid);
				if (a == CaveKind::Noodle) { ++noodles; retainedNoodles += b != CaveKind::Solid; }
				// Signed interpolation agrees at grid nodes, across negative coordinates/borders.
				if (x%step == 0 && y%step == 0 && z%step == 0) assert(a == b);
				assert((a == CaveKind::Entrance) == (b == CaveKind::Entrance));
				if (y <= 1) assert(b == CaveKind::Solid);
				if (y >= 158) assert(b == CaveKind::Solid || b == CaveKind::Entrance);
			}
		}
		assert(changed < 64*128*161/30); // Under 3.4% of the sampled volume changes.
		assert(noodles > 0 && retainedNoodles*10 > noodles*9);
		assert(counts[1] > 0 && counts[2] > 0 && counts[3] > 0);
		auto expected = fingerprint(sampled);
		assert(fingerprint(sampled,true) == expected);
		auto job = std::async(std::launch::async,[&] { return fingerprint(sampled,true); });
		assert(job.get() == expected);
		// Reuse the same storage with different seeds/settings, retaining thread-local caches.
		reused.emplace(seed,settings);
		assert(fingerprint(*reused) == expected);
		settings.warpStrength = 2;
		CaveGenerator warped(seed,settings);
		auto warpedExpected = fingerprint(warped);
		reused.emplace(seed,settings);
		assert(fingerprint(*reused,true) == warpedExpected);
		std::cout << "Seed " << seed << ", sampling " << step << ": " << changed
			<< " changed voxels, " << retainedNoodles << '/' << noodles << " noodle voxels retained\n";
	}
}
