#include "CaveGenerator.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <iostream>
#include <vector>

int main() {
	std::array<std::vector<double>,3> timings;
	std::array<size_t,3> air{};
	// Rotate mode order to reduce warmup/CPU-frequency bias. Columns are
	// prepared outside the timer, as production shares them across layers.
	for (int round = 0; round < 5; ++round) for (int mode = 0; mode < 3; ++mode) {
		int index = (mode+round)%3;
		CaveGenerationSettings settings;
		settings.noiseSampleStep = 1 << index;
		CaveGenerator caves(42,settings);
		std::vector<CaveGenerator::Column> columns;
		columns.reserve(128*128);
		for (int z = -64; z < 64; ++z) for (int x = -64; x < 64; ++x)
			columns.push_back(caves.prepareColumn(x,z,160));
		auto start = std::chrono::steady_clock::now();
		size_t count = 0;
		for (int bz = -64; bz < 64; bz += 32) for (int bx = -64; bx < 64; bx += 32)
		for (int by = 0; by < 160; by += 32)
		for (int z = bz; z < bz+32; ++z) for (int x = bx; x < bx+32; ++x) {
			const auto& column = columns[(z+64)*128+x+64];
			for (int y = by; y < by+32; ++y) count += caves.classify(column,y) != CaveKind::Solid;
		}
		timings[index].push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count());
		air[index] = count;
	}
	std::cout << "Seed 42, 2,621,440 voxel decisions in 32^3 subchunks, median of five runs\n";
	for (int index = 0; index < 3; ++index) {
		auto& values = timings[index];
		std::sort(values.begin(),values.end());
		std::cout << "step " << (1 << index) << ": " << values[2] << " ms, " << air[index] << " air voxels\n";
	}
}
