#pragma once

#include "ft_vox.hpp"
#include "define.hpp"
#include "SplineInterpolator.hpp"

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
			double *caveMap = nullptr;
			ivec2	position;
			double	heighest = std::numeric_limits<double>::min();
			double	lowest = std::numeric_limits<double>::max();
			double	resolution = 1;
			int		size = CHUNK_SIZE;
			~PerlinMap() {
				if (heightMap) delete [] heightMap;
				if (caveMap) delete [] caveMap;};
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
	private:
		double singleNoise(double x, double y) const;
		double fade(double t) const;
		double lerp(double a, double b, double t) const;
		double grad(int hash, double x, double y) const;
		double getContinentalNoise(ivec2 pos);
		double getErosionNoise(ivec2 pos);
		double getOceanNoise(ivec2 pos);
		double getHeight(ivec2 pos);
		double getPeaksValleysNoise(ivec2 pos);

		size_t _seed;
		NoiseData _data;
		std::vector<int> _permutation;
		std::unordered_map<ivec2, PerlinMap*, ivec2_hash>	_perlinMaps;
		std::mutex				_perlinMutex;
		SplineData spline;
};

typedef NoiseGenerator::PerlinMap PerlinMap;