#pragma once

#include "ft_vox.hpp"
#include "define.hpp"
#include "SplineInterpolator.hpp"
#include <unordered_map>

// Update nb_biomes when adding a new one for debug box
#define NB_BIOMES 7
enum Biome {
	PLAINS,
	DESERT,
	SNOWY,
	MOUNTAINS,
	FOREST,
	OCEAN,
	BEACH
};

struct NoiseData {
	double amplitude = 1.0;
	double frequency = 0.001;
	double persistance = 0.5;
	double lacunarity = 2.0;
	size_t nb_octaves = 5;
};

struct SplineData {
	SplineInterpolator continentalSpline;
	SplineInterpolator erosionSpline;
	SplineInterpolator peaksValleysSpline;
};

class NoiseGenerator {
	public:
		struct PerlinMap {
			double *heightMap = nullptr;
			double *treeMap = nullptr;
			double *grassMap = nullptr;           // probability for short grass
			double *flowerMask = nullptr;        // overall flower density mask
			double *flowerR = nullptr;           // cluster weights per color
			double *flowerY = nullptr;
			double *flowerB = nullptr;
			Biome *biomeMap = nullptr;
			ivec2	position;
			double	heighest = std::numeric_limits<double>::min();
			double	lowest = std::numeric_limits<double>::max();
			double	resolution = 1;
			int		size = CHUNK_SIZE;
			~PerlinMap() {
				if (heightMap) { delete [] heightMap; heightMap = nullptr; }
				if (biomeMap)  { delete [] biomeMap;  biomeMap  = nullptr; }
				if (treeMap)   { delete [] treeMap;   treeMap   = nullptr; }
				if (grassMap)  { delete [] grassMap;  grassMap  = nullptr; }
				if (flowerMask){ delete [] flowerMask;flowerMask= nullptr; }
				if (flowerR)   { delete [] flowerR;   flowerR   = nullptr; }
				if (flowerY)   { delete [] flowerY;   flowerY   = nullptr; }
				if (flowerB)   { delete [] flowerB;   flowerB   = nullptr; }
			};
		};
	public:
		NoiseGenerator(size_t seed);
		~NoiseGenerator();
		double noise(double x, double y) const;
		void updatePerlinMapResolution(PerlinMap *map, int resolution);
		PerlinMap *addPerlinMap(ivec2 &pos, int size, int resolution);
		PerlinMap *getPerlinMap(ivec2 &pos, int resolution = 1);
		void clearPerlinMaps(void);
		void setSeed(size_t seed);
		const size_t &getSeed() const;
		void setNoiseData(const NoiseData &data);
		void removePerlinMap(int x, int z);
		ivec2 getBorderWarping(double x, double z);
		double getHeight(ivec2 pos);
		double getContinentalNoise(ivec2 pos);

		// Biomes
		ivec2 getBiomeBorderWarping(int x, int z);
		double getTemperatureNoise(ivec2 pos);
		double getHumidityNoise(ivec2 pos);
		Biome getBiome(ivec2 pos, double height);
	private:
		double singleNoise(double x, double y) const;
		double fade(double t) const;
		double lerp(double a, double b, double t) const;
		double grad(int hash, double x, double y) const;
		double getErosionNoise(ivec2 pos);
		double getOceanNoise(ivec2 pos);
		double getPeaksValleysNoise(ivec2 pos);
		double getTreeNoise(ivec2 pos);
		double getTreeProbability(ivec2 pos);
		void   buildTreeMap(PerlinMap* map, int resolution);
		// Plants/flowers maps
		double getGrassNoise(ivec2 pos);
		double getFlowerBaseNoise(ivec2 pos);
		double getFlowerClusterNoise(ivec2 pos, int channel);
		void   buildPlantMaps(PerlinMap* map, int resolution);

		size_t _seed;
		NoiseData _data;
		std::vector<int> _permutation;
		std::unordered_map<ivec2, PerlinMap*, ivec2_hash>	_perlinMaps;
		std::mutex				_perlinMutex;
		SplineData spline;
};

typedef NoiseGenerator::PerlinMap PerlinMap;
