#include "NoiseGenerator.hpp"

NoiseGenerator::NoiseGenerator(size_t seed): _seed(seed)
{
	// Initialize permutation table
	std::mt19937 generator(seed);
	std::vector<int> p(256);
	for (int i = 0; i < 256; i++) p[i] = i;
	std::shuffle(p.begin(), p.end(), generator);
	_permutation.resize(512);
	for (int i = 0; i < 512; i++) _permutation[i] = p[i % 256];

	// Spline points of the height for each noise value (from -1.0 to 1.0)
	// Continentalness: Base terrain height with plateaux
	// Erosion: Erosion simili for somewhat smoothed out areas
	// Peaks: High height values for mountains and peaks generation
	std::vector<Point> continentalPoints = {{-1.0, -10.0}, {-0.4, -10.0}, {-0.3, 50.0}, {-0.1, 50.0}, {-0.05, 100.0}, {0, 100.0}, {0.1, 115}, {0.3, 125.0}, {1.0, 145.0}};
	std::vector<Point> erosionPoints = {{-1.0, 150.0}, {-0.8, 100.0}, {-0.5, 75.0}, {0.0, 25.0}, {0.3, 22.5}, {0.4, 20.0}, {0.5, 10.0}, {0.6, 15.0}, {0.7, 20.0}, {1.0, 10.0}};
	std::vector<Point> peaksPoints = {{-1.0, -15.0}, {-0.8, 25.0}, {-0.4, 75.0}, {0.0, 125.0}, {0.4, 225.0}, {1.0, 350.0}};

	spline.continentalSpline.setPoints(continentalPoints);
	spline.erosionSpline.setPoints(erosionPoints);
	spline.peaksValleysSpline.setPoints(peaksPoints);
}

NoiseGenerator::~NoiseGenerator()
{
	clearPerlinMaps();
}

void NoiseGenerator::setSeed(size_t seed)
{
	std::cout << "Setting seed to: " << seed << std::endl;
	_seed = seed;
}

void NoiseGenerator::clearPerlinMaps(void)
{
	for (auto &map : _perlinMaps)
	{
		if (map.second && map.second->heightMap)
		{
			delete [] map.second->heightMap;
			map.second->heightMap = nullptr;
		}
		if (map.second)
		{
			delete map.second;
			map.second = nullptr;
		}
	}
	_perlinMaps.clear();
}

void NoiseGenerator::setNoiseData(const NoiseData &data)
{
	_data = data;
}

double NoiseGenerator::getContinentalNoise(vec2 pos)
{
	double _noise = 0.0;
	NoiseData nData = {
		0.9, // amplitude
		0.002, // frequency
		0.5, // persistance
		2.0, // lacunarity
		6 // nb_octaves
	};

	setNoiseData(nData);
	_noise = noise(pos.x, pos.y);
	setNoiseData(NoiseData());
	return _noise;
}

double NoiseGenerator::getErosionNoise(vec2 pos)
{
	double _noise = 0.0;
	NoiseData nData = {
		0.9, // amplitude
		0.00005, // frequency
		0.2, // persistance
		2.0, // lacunarity
		4 // nb_octaves
	};

	_data = nData;
	_noise = noise(pos.x, pos.y);
	setNoiseData(NoiseData());
	return _noise;
}

double NoiseGenerator::getPeaksValleysNoise(vec2 pos)
{
	double _noise = 0.0;
	NoiseData nData = {
		0.7, // amplitude
		0.00099, // frequency
		0.5, // persistance
		2.0, // lacunarity
		5 // nb_octaves
	};

	_data = nData;
	_noise = noise(pos.x, pos.y);
	setNoiseData(NoiseData());
	return _noise;
}

double NoiseGenerator::getOceanNoise(vec2 pos)
{
	double _noise = 0.0;
	NoiseData nData = {
		1.0,  // amplitude
		0.0008, // frequency
		0.6,  // persistence
		1.8,  // lacunarity
		5     // nb_octaves
	};

	setNoiseData(nData);
	_noise = noise(pos.x, pos.y);
	setNoiseData(NoiseData());
	return _noise;
}

vec2 NoiseGenerator::getBorderWarping(double x, double z)
{
	NoiseData nData = {
		1.0,  // amplitude
		0.0005, // frequency
		0.6,  // persistence
		2.5,  // lacunarity
		5     // nb_octaves
	};

	setNoiseData(nData);
	double noiseX = noise(x, z);
	double noiseY = noise(z, x);
	setNoiseData(NoiseData());
	vec2 offset;
	offset.x = x + (noiseX * CHUNK_SIZE);
	offset.y = z + (noiseY * CHUNK_SIZE);
	return offset;
}

double smoothBlend(double a, double b, double blendFactor)
{
	// Smoothstep
	blendFactor = blendFactor * blendFactor * (3.0 - 2.0 * blendFactor);
	return a * (1.0 - blendFactor) + b * blendFactor;
}

double NoiseGenerator::getHeight(vec2 pos)
{
	pos = getBorderWarping(pos.x, pos.y);
	double continentalNoise = getContinentalNoise(pos);
	double surfaceHeight = spline.continentalSpline.interpolate(continentalNoise);
	double erosionNoise = getErosionNoise(pos);
	double erosionHeight = spline.erosionSpline.interpolate(erosionNoise);
	double erosionMask = (erosionNoise + 1.0) * 0.5;
	double peaksNoise = getPeaksValleysNoise(pos);
	double peaksHeight = spline.peaksValleysSpline.interpolate(peaksNoise) * 2.0;
	double peaksMask = (peaksNoise + 1.0) * 0.5;

	// Calculate ocean noise and mask
	// double oceanNoise = 0.2 * getOceanNoise(pos);
	// double oceanMask = (oceanNoise + 1.0) * 0.5; // Normalize to 0-1
	// double oceanThreshold = 0.48;  // Controls ocean frequency (lower = more oceans)

	// Base terrain height
	double height;
	height = smoothBlend(surfaceHeight, erosionHeight, erosionMask);
	height = smoothBlend(height, peaksHeight, peaksMask);

	// // Apply ocean mask
	// if (oceanMask < oceanThreshold)
	// {
	// 	//std::cout << "Ocean" << std::endl;
	// 	double blendFactor = (oceanThreshold - oceanMask) / oceanThreshold; // Blend smoothly
	// 	blendFactor = glm::clamp(blendFactor, 0.0, 1.0);
	// 	height = smoothBlend(height, -50.0, blendFactor); // 50.0 is the ocean level
	// }

	//height = pow(height, 1.05); // Slightly bias towards higher values
	return height;
}

void NoiseGenerator::updatePerlinMapResolution(PerlinMap *map, int resolution)
{
	if (!map || resolution > map->resolution)
		return ;
	// Todo: Not recalculate height that already exist
	// while (map->resolution > resolution)
	// {
	// 	for (int x = 0; x < map->size; x += map->resolution)
	// 		for (int z = 0; z < map->size; z += map->resolution)
	// 		{
	// 			map->heightMap[z * map->size + x] = getHeight({(map->position.x * map->size) + x, (map->position.y * map->size) + z});
	// 			if (map->heightMap[z * map->size + x] > map->heighest)
	// 				map->heighest = map->heightMap[z * map->size + x];
	// 			if (map->heightMap[z * map->size + x] < map->lowest)
	// 				map->lowest = map->heightMap[z * map->size + x];
	// 		}
	// 	map->resolution /= 2;
	// }

	for (int x = 0; x < map->size; x += resolution)
		for (int z = 0; z < map->size; z += resolution)
		{
			map->heightMap[z * map->size + x] = getHeight({(map->position.x * map->size) + x, (map->position.y * map->size) + z});
			if (map->heightMap[z * map->size + x] > map->heighest)
				map->heighest = map->heightMap[z * map->size + x];
			if (map->heightMap[z * map->size + x] < map->lowest)
				map->lowest = map->heightMap[z * map->size + x];
		}
	map->resolution = resolution;
}

PerlinMap *NoiseGenerator::addPerlinMap(ivec2 &pos, int size, int resolution)
{
	PerlinMap *newMap = new PerlinMap();
	newMap->size = size;
	newMap->heightMap = new double[size * size];
	newMap->caveMap = new double[size * size * size];
	newMap->resolution = resolution;
	newMap->position = pos;
	newMap->heighest = 0;
	newMap->lowest = 256;

	for (int x = 0; x < size; x += resolution)
		for (int z = 0; z < size; z += resolution)
		{
			newMap->heightMap[z * size + x] = getHeight({(pos.x * size) + x, (pos.y * size) + z});
			if (newMap->heightMap[z * size + x] > newMap->heighest)
				newMap->heighest = newMap->heightMap[z * size + x];
			if (newMap->heightMap[z * size + x] < newMap->lowest)
				newMap->lowest = newMap->heightMap[z * size + x];
		}
	_perlinMaps[pos] = newMap;
	return (newMap);
}


void NoiseGenerator::removePerlinMap(int x, int z)
{
	std::lock_guard<std::mutex> lock(_perlinMutex);
	auto it = _perlinMaps.find({x, z});
	auto itend = _perlinMaps.end();
	PerlinMap *map = (*it).second;
	if (it != itend)
	{
		if (map && map->heightMap)
		{
			delete [] map->heightMap;
			map->heightMap = nullptr;
		}
		if (map)
		{
			delete map;
			map = nullptr;
		}
		_perlinMaps.erase({x, z});
	}
}

PerlinMap *NoiseGenerator::getPerlinMap(ivec2 &pos, int resolution)
{
	std::lock_guard<std::mutex> lock(_perlinMutex);
	auto it = _perlinMaps.find(pos);
	auto itend = _perlinMaps.end();
	if (it != itend)
	{
		if ((*it).second->resolution > resolution)
			updatePerlinMapResolution((*it).second, resolution);
		return ((*it).second);
	}
	return addPerlinMap(pos, CHUNK_SIZE, resolution);
}

// Layered perlin noise samples by octaves number
double NoiseGenerator::noise(double x, double y) const
{
	double total = 0.0;
	double amplitude = _data.amplitude;
	double frequency = _data.frequency;
	double maxValue = 0.0; // Used for normalizing

	for (size_t i = 0; i < _data.nb_octaves; i++) {
		total += singleNoise(x * frequency, y * frequency) * amplitude;
		maxValue += amplitude;

		amplitude *= _data.persistance;
		frequency *= _data.lacunarity;
	}
	return total / maxValue; // Normalize
}

double NoiseGenerator::singleNoise(double x, double y) const
{
	int X = (int)std::floor(x) & 255;
	int Y = (int)std::floor(y) & 255;

	x -= std::floor(x);
	y -= std::floor(y);

	double u = fade(x);
	double v = fade(y);

	int A = _permutation[X] + Y;
	int B = _permutation[X + 1] + Y;

	return lerp(
		lerp(grad(_permutation[A], x, y), grad(_permutation[B], x - 1, y), u),
		lerp(grad(_permutation[A + 1], x, y - 1), grad(_permutation[B + 1], x - 1, y - 1), u),
		v
	);
}

double NoiseGenerator::fade(double t) const
{
	return t * t * t * (t * (t * 6 - 15) + 10);
}

double NoiseGenerator::lerp(double a, double b, double t) const
{
	return a + t * (b - a);
}

double NoiseGenerator::grad(int hash, double x, double y) const
{
	int h = hash & 3; // Only 4 gradients
	double u = h < 2 ? x : y;
	double v = h < 2 ? y : x;
	return ((h & 1) == 0 ? u : -u) + ((h & 2) == 0 ? v : -v);
}
