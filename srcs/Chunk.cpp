#include "Chunk.hpp"

Chunk::Chunk(int x, int y, int z, NoiseGenerator::PerlinMap *perlinMap, World &world, TextureManager &textManager) : _world(world), _textManager(textManager)
{
	_position = vec3(x, y, z);
	_blocks.resize(CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE);
	memcpy(_perlinMap, perlinMap->map, (sizeof(double) * perlinMap->size * perlinMap->size));
	bzero(_blocks.data(), _blocks.size());
	loadHeight();
	loadBiome();
}

void Chunk::loadHeight()
{
	if (loaded) return ;
	loaded = true;
	for (int y = 0; y < CHUNK_SIZE ; y++)
	{
		for (int x = 0; x < CHUNK_SIZE ; x++)
		{
			for (int z = 0; z < CHUNK_SIZE ; z++)
			{
				double height = _perlinMap[z * CHUNK_SIZE + x];
				size_t maxHeight = (size_t)(height);
				if (y + _position.y * CHUNK_SIZE <= maxHeight)
					_blocks[x + (z * CHUNK_SIZE) + (y * CHUNK_SIZE * CHUNK_SIZE)] = 'S';
				else
					_blocks[x + (z * CHUNK_SIZE) + (y * CHUNK_SIZE * CHUNK_SIZE)] = 'A';
			}
		}
	}
}

vec2 Chunk::getBorderWarping(double x, double z, NoiseGenerator &noise_gen) const
{
	double noiseX = noise_gen.noise(x, z);
	double noiseY = noise_gen.noise(z, x);
	vec2 offset;
	offset.x = noiseX * 15.0;
	offset.y = noiseY * 15.0;
	return offset;
}

double Chunk::getContinentalNoise(vec2 pos, NoiseGenerator &noise_gen)
{
	double noise = 0.0;
	NoiseData nData = {
		1.0, // amplitude
		0.001, // frequency
		0.8, // persistance
		4.0, // lacunarity
		4 // nb_octaves
	};

	noise_gen.setNoiseData(nData);
	noise = noise_gen.noise(pos.x, pos.y);
	noise_gen.setNoiseData(NoiseData());
	return noise;
}

double Chunk::getMinHeight(vec2 pos, NoiseGenerator &noise_gen)
{
	std::vector<Point> splinePoints = {{-1.0, 50.0}, {0.3, 100.0}, {0.4, 150.0}, {1.0, 150.0}};
	double continentalNoise = getContinentalNoise(pos, noise_gen);
	SplineInterpolator spline(splinePoints);

	double splineResult = spline.interpolate(continentalNoise);
	return splineResult;
}

void Chunk::loadBiome()
{
	for (int x = 0; x < CHUNK_SIZE ; x++)
	{
		for (int z = 0; z < CHUNK_SIZE ; z++)
		{
			bool surfaceFound = false;
			for (int y = CHUNK_SIZE - 1; y >= 0; y--)
			{
				int index = x + (z * CHUNK_SIZE) + (y * CHUNK_SIZE * CHUNK_SIZE);
				if (!surfaceFound && _blocks[index] == 'S')
				{
					_blocks[index] = 'G';
					surfaceFound = true;
					for (int dirtLayer = 1; dirtLayer <= 5; dirtLayer++)
					{
						int dirtIndex = x + (z * CHUNK_SIZE) + ((y - dirtLayer) * CHUNK_SIZE * CHUNK_SIZE);
						if (y - dirtLayer >= 0 && _blocks[dirtIndex] == 'S')
							_blocks[dirtIndex] = 'D';
						else
							break ;
					}
				}
			}
		}
	}
}

Chunk::~Chunk()
{
	loaded = false;
}

char Chunk::getBlock(int x, int y, int z)
{
	if (x >= CHUNK_SIZE || y >= CHUNK_SIZE || z >= CHUNK_SIZE || x < 0 || y < 0 || z < 0)
		return 'D';
	return _blocks[x + (z * CHUNK_SIZE) + (y * CHUNK_SIZE * CHUNK_SIZE)];
}

void Chunk::addBlock(int blockX, int blockY, int blockZ, TextureType down, TextureType up, TextureType north, TextureType south, TextureType east, TextureType west)
{
	int x = _position.x * CHUNK_SIZE + blockX;
	int y = _position.y * CHUNK_SIZE + blockY;
	int z = _position.z * CHUNK_SIZE + blockZ;

	if ((blockY == 0 && _world.getBlock(x, y - 1, z) == 'A') || ((blockY != 0 && _blocks[blockX + (blockZ * CHUNK_SIZE) + ((blockY - 1) * CHUNK_SIZE * CHUNK_SIZE)] == 'A')))
		_textManager.addTextureVertex(down, DOWN, x, y, z);
	if ((blockY == 15 && _world.getBlock(x, y + 1, z) == 'A') || ((blockY != 15 && _blocks[blockX + (blockZ * CHUNK_SIZE) + ((blockY + 1) * CHUNK_SIZE * CHUNK_SIZE)] == 'A')))
		_textManager.addTextureVertex(up, UP, x, y, z);
	if ((blockZ == 0 && _world.getBlock(x, y, z - 1) == 'A') || ((blockZ != 0 && _blocks[blockX + ((blockZ - 1) * CHUNK_SIZE) + (blockY * CHUNK_SIZE * CHUNK_SIZE)] == 'A')))
		_textManager.addTextureVertex(north, NORTH, x, y, z);
	if ((blockZ == 15 && _world.getBlock(x, y, z + 1) == 'A') || ((blockZ != 15 && _blocks[blockX + ((blockZ + 1) * CHUNK_SIZE) + (blockY * CHUNK_SIZE * CHUNK_SIZE)] == 'A')))
		_textManager.addTextureVertex(south, SOUTH, x, y, z);
	if ((blockX == 0 && _world.getBlock(x - 1, y, z) == 'A') || ((blockX != 0 && _blocks[(blockX - 1) + (blockZ * CHUNK_SIZE) + (blockY * CHUNK_SIZE * CHUNK_SIZE)] == 'A')))
		_textManager.addTextureVertex(east, EAST, x, y, z);
	if ((blockX == 15 && _world.getBlock(x + 1, y, z) == 'A') || ((blockX != 15 && _blocks[(blockX + 1) + (blockZ * CHUNK_SIZE) + (blockY * CHUNK_SIZE * CHUNK_SIZE)] == 'A')))
		_textManager.addTextureVertex(west, WEST, x, y, z);
}

vec3 Chunk::getPosition()
{
	return _position;
}

void Chunk::sendFacesToDisplay()
{
	for (int x = 0; x < CHUNK_SIZE; x++)
	{
		for (int y = 0; y < CHUNK_SIZE; y++)
		{
			for (int z = 0; z < CHUNK_SIZE; z++)
			{
				switch (_blocks[x + (z * CHUNK_SIZE) + (y * CHUNK_SIZE * CHUNK_SIZE)])
				{
					case 'A':
						break;
					case 'D':
						addBlock(x, y, z, T_DIRT, T_DIRT, T_DIRT, T_DIRT, T_DIRT, T_DIRT);
						break;
					case 'S':
						addBlock(x, y, z, T_STONE, T_STONE, T_STONE, T_STONE, T_STONE, T_STONE);
						break;
					case 'G':
						addBlock(x, y, z, T_DIRT, T_GRASS_TOP, T_GRASS_SIDE, T_GRASS_SIDE, T_GRASS_SIDE, T_GRASS_SIDE);
						break;
					default :
						break;
				}
			}
		}
	}
}
