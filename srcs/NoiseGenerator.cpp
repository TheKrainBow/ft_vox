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
	std::vector<Point> erosionPoints = {{-1.0, 150.0}, {-0.8, 100.0}, {-0.5, 75.0}, {0.0, 25.0}, {0.3, 22.5}, {0.4, 20.0}, {0.5, 50.0}, {0.6, 50.0}, {0.7, 20.0}, {1.0, 10.0}};
	std::vector<Point> peaksPoints = {{-1.0, -15.0}, {-0.8, 15.0}, {-0.4, 45.0}, {0.0, 50.0}, {0.4, 125.0}, {1.0, 200.0}};

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
		if (map && map->map)
		{
			delete [] map->map;
			map->map = nullptr;
		}
		if (map)
		{
			delete map;
			map = nullptr;
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
		4 // nb_octaves
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
		1.0, // amplitude
		0.002, // frequency
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
		0.001, // frequency
		0.5, // persistance
		2.5, // lacunarity
		3 // nb_octaves
	};

	_data = nData;
	_noise = noise(pos.x, pos.y);
	setNoiseData(NoiseData());
	return _noise;
}

vec2 NoiseGenerator::getBorderWarping(double x, double z) const
{
	double noiseX = noise(x, z);
	double noiseY = noise(z, x);
	vec2 offset;
	offset.x = x + (noiseX * CHUNK_SIZE);
	offset.y = z + (noiseY * CHUNK_SIZE);
	return offset;
}

int NoiseGenerator::getHeight(vec2 pos)
{
	pos = getBorderWarping(pos.x, pos.y);
	double continentalNoise = getContinentalNoise(pos);
	double surfaceHeight = spline.continentalSpline.interpolate(continentalNoise);
	double erosionNoise = getErosionNoise(pos);
	double erosionHeight = spline.erosionSpline.interpolate(erosionNoise);
	double erosionMask = (erosionNoise + 1.0) * 0.5;
	double peaksNoise = getPeaksValleysNoise(pos);
	double peaksHeight = spline.peaksValleysSpline.interpolate(peaksNoise) * 4.5;
	double peaksMask = (peaksNoise + 1.0) * 0.5;

	int height;
	height = surfaceHeight * (1.0 - erosionMask) + erosionHeight * erosionMask;
	height = height * (1.0 - peaksMask) + peaksHeight * peaksMask;
	//height = clamp(height, -255, 500);
	return height;
}

PerlinMap *NoiseGenerator::addPerlinMap(int startX, int startZ, int size, int resolution)
{
	PerlinMap *newMap = new PerlinMap();
	newMap->size = size;
	newMap->map = new double[size * size];
	newMap->resolution = resolution;
	newMap->position = vec2(startX, startZ);

	for (int x = 0; x < size; x += resolution)
		for (int z = 0; z < size; z += resolution)
		{
			newMap->map[z * size + x] = getHeight({(startX * size) + x, (startZ * size) + z});
			if (newMap->map[z * size + x] > newMap->heighest)
				newMap->heighest = newMap->map[z * size + x];
			if (newMap->map[z * size + x] < newMap->lowest)
				newMap->lowest = newMap->map[z * size + x];
		}
	_perlinMaps.push_back(newMap);
	return (newMap);
}

PerlinMap *NoiseGenerator::getPerlinMap(int x, int y)
{
	std::lock_guard<std::mutex> lock(_perlinMutex);
	for (auto &map : _perlinMaps)
	{
		if (map->position.x == x && map->position.y == y)
			return (map);
	}
	return addPerlinMap(x, y, CHUNK_SIZE, 1);
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
