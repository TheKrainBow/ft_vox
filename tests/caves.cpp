#include "CaveGenerator.hpp"
#include <array>
#include <cassert>
#include <chrono>
#include <fstream>
#include <future>
#include <iostream>

using Counts = std::array<size_t, 5>;
static uint64_t chunk(const CaveGenerator& caves, int cx, int cz) {
	uint64_t hash = 1469598103934665603ULL;
	for (int z = 0; z < 32; ++z) for (int x = 0; x < 32; ++x) {
		auto column = caves.prepareColumn(cx * 32 + x, cz * 32 + z, 160);
		for (int y = 0; y <= 160; ++y) {
			hash ^= static_cast<unsigned>(caves.classify(column, y));
			hash *= 1099511628211ULL;
		}
	}
	return hash;
}

int main(int argc, char** argv) {
	uint64_t seed = argc > 2 ? std::stoull(argv[2]) : 42;
	CaveGenerator caves(seed), repeat(seed), other(seed + 1);
	for (int cz = -1; cz <= 1; ++cz) for (int cx = -1; cx <= 1; ++cx) {
		auto job = std::async(std::launch::async, [&caves, cx, cz] { return chunk(caves, cx, cz); });
		auto expected = chunk(repeat, cx, cz);
		assert(job.get() == expected);
		assert(chunk(other, cx, cz) != expected);
	}
	Counts counts{};
	size_t mouths = 0, crossings = 0;
	std::array<size_t, 5> longest{};
	const int extent = 256, height = 160;
	std::ofstream slice;
	if (argc > 1) {
		slice.open(argv[1]);
		slice << "P3\n" << extent << ' ' << height << "\n255\n";
		const char* colors[] = {"35 38 42", "230 165 65", "80 180 240", "180 110 230", "80 230 130"};
		for (int y = height; y > 0; --y) {
			for (int x = -extent/2; x < extent/2; ++x)
				slice << colors[static_cast<int>(caves.classify(caves.prepareColumn(x, 17, height), y))] << '\n';
		}
	}
	auto start = std::chrono::steady_clock::now();
	for (int z = -extent/2; z < extent/2; ++z) for (int x = -extent/2; x < extent/2; ++x) {
		auto column = caves.prepareColumn(x, z, height);
		assert(caves.classify(column, 0) == CaveKind::Solid);
		assert(caves.classify(column, 1) == CaveKind::Solid);
		assert(caves.classify(column, height + 1) == CaveKind::Solid);
		assert(caves.classify(caves.prepareColumn(x, z, 800), 640) == CaveKind::Solid);
		for (int y = 1; y <= height; ++y) {
			auto kind = caves.classify(column, y);
			++counts[static_cast<int>(kind)];
			if (y >= height - 2) assert(kind == CaveKind::Solid || kind == CaveKind::Entrance);
			if (y == height && kind == CaveKind::Entrance) ++mouths;
			if (kind != CaveKind::Solid && x % 32 == 0 && caves.isAir(x-1, y, z, height)) ++crossings;
		}
	}
	// Horizontal runs measure scale classes without requiring exact noise snapshots.
	for (int z = -64; z < 64; z += 4) for (int y = 20; y < 140; y += 3) {
		int previous = 0; size_t run = 0;
		for (int x = -128; x < 128; ++x) {
			int kind = static_cast<int>(caves.classify(caves.prepareColumn(x,z,height),y));
			run = kind == previous ? run + 1 : 1;
			longest[kind] = std::max(longest[kind], run); previous = kind;
		}
	}
	double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
	double total = extent * extent * height;
	for (size_t i = 0; i < counts.size(); ++i)
		std::cout << "kind " << i << ": " << counts[i] * 100.0 / total << "% longest X run " << longest[i] << '\n';
	assert(counts[1] > 1000 && counts[2] > counts[1] && counts[3] > 100);
	assert(counts[3] < counts[2] / 3);
	assert(counts[0] / total > 0.70 && counts[0] / total < 0.98);
	assert(longest[1] >= 10 && longest[2] >= 7 && longest[3] >= 2);
	assert(mouths > 0 && mouths < extent * extent / 100);
	assert(crossings > 100);
	std::cout << "Surface mouths: " << mouths << '/' << extent*extent << ", border crossings: " << crossings
		<< ", scan seconds: " << seconds << '\n';
}
