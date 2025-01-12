#include "Chunk.hpp"

Chunk::Chunk(int chunkX, int chunkZ, NoiseGenerator &noise_gen, BiomeGenerator &biome_gen)
{
	_position = vec2(chunkX, chunkZ);
	bzero(_blocks, CHUNK_SIZE * sizeof(ABlock *));

	double relativePosX = _position.x * 16.0;
	double relativePosZ = _position.y * 16.0;
	double noise;
	vec2 offsetBorder = {0.0, 0.0};
	double remappedNoise;
	size_t maxHeight;
	(void)biome_gen;
	BiomeType type = PLAINS;
	// Generate terrain. (Must be refactored using Perlin Noise)
	for (int x = 0; x < 16; x++)
	{
		for (int z = 0; z < 16; z++)
		{
			offsetBorder = getBorderWarping(relativePosX + x, relativePosZ + z, noise_gen);
			//type = biome_gen.findClosestBiomes(offsetBorder.x + relativePosX + x, offsetBorder.y + relativePosZ + z);
			// offsetBorder.x = relativePosX + x;
			// offsetBorder.y = relativePosZ + z;
			double minHeight = 25.0;
			minHeight = getMinHeight(offsetBorder, noise_gen);
			noise = noise_gen.noise(relativePosX + x, relativePosZ + z);
			remappedNoise = 100.0 + noise * minHeight;
			maxHeight = (size_t)(remappedNoise);
			for (size_t y = 0; y < maxHeight / 2; y++)
				_blocks[x + (z * CHUNK_SIZE_X) + (y * CHUNK_SIZE_X * CHUNK_SIZE_Z)] = new Stone((chunkX * CHUNK_SIZE_X) + x, y, (chunkZ * CHUNK_SIZE_Z) + z);
			if (type == PLAINS)
			{
				for (size_t y = maxHeight / 2; y < maxHeight; y++)
					_blocks[x + (z * CHUNK_SIZE_X) + (y * CHUNK_SIZE_X * CHUNK_SIZE_Z)] = new Dirt((chunkX * CHUNK_SIZE_X) + x, y, (chunkZ * CHUNK_SIZE_Z) + z);
				_blocks[x + (z * CHUNK_SIZE_X) + (maxHeight * CHUNK_SIZE_X * CHUNK_SIZE_Z)] = new Grass((chunkX * CHUNK_SIZE_X) + x, maxHeight, (chunkZ * CHUNK_SIZE_Z) + z);
			}
			else if (type == DESERT)
			{
				for (size_t y = maxHeight / 2; y <= maxHeight; y++)
					_blocks[x + (z * CHUNK_SIZE_X) + (y * CHUNK_SIZE_X * CHUNK_SIZE_Z)] = new Sand((chunkX * CHUNK_SIZE_X) + x, y, (chunkZ * CHUNK_SIZE_Z) + z);
			}
		}
	}
	displayCheckFaces();
}

vec2 Chunk::getBorderWarping(double x, double z, const NoiseGenerator &noise_gen) const
{
	double noise = noise_gen.noise(x, z);
	double offset = (int)(((noise + 1.0) * 0.5) * 255.0);
	return {offset, offset};
}

double Chunk::getContinentalNoise(vec2 pos, NoiseGenerator &noise_gen)
{
	double noise = 0.0;
	NoiseData nData = {
		2.0, // amplitude
		0.01, // frequency
		0.5, // persistance
		2.0, // lacunarity
		4 // nb_octaves
	};

	noise_gen.setNoiseData(nData);
	noise = noise_gen.noise(pos.x, pos.y);
	noise_gen.setNoiseData(NoiseData());
	return noise;
}

double Chunk::getMinHeight(vec2 pos, NoiseGenerator &noise_gen)
{
	double heightLow = 25.0;
	double heightMid = 50.0;
	double heightHigh = 100.0;

	double continentalNoise = getContinentalNoise(pos, noise_gen);
	if (continentalNoise <= 0.3)
		return heightLow;
	else if (continentalNoise < 0.9)
	{
		// Interpolate between heightLow and heightMid
		double t = (continentalNoise - 0.3) / (0.9 - 0.3);
		return heightLow * (1.0 - t) + heightMid * t;
	}
	double t = (continentalNoise - 0.9) / (1.0 - 0.9);
	return heightMid * (1.0 - t) + heightHigh * t;
}

void Chunk::displayCheckFaces() const
{
	// Check for faces to display (Only for current chunk)
	for (int x = 0; x < CHUNK_SIZE_X; ++x) {
		for (int z = 0; z < CHUNK_SIZE_Z; ++z) {
			for (int y = 0; y < CHUNK_SIZE_Y; ++y) {
				int index = x + z * CHUNK_SIZE_X + y * CHUNK_SIZE_X * CHUNK_SIZE_Z;
				if (!_blocks[index])
						continue ;
				int state = 0;
				bool displayDown = (y == 0) || _blocks[x + z * CHUNK_SIZE_X + (y - 1) * CHUNK_SIZE_X * CHUNK_SIZE_Z] == NULL;
				bool displayUp = (y == 255) || _blocks[x + z * CHUNK_SIZE_X + (y + 1) * CHUNK_SIZE_X * CHUNK_SIZE_Z] == NULL;
				bool displayRight = (x == 0) || _blocks[(x - 1) + z * CHUNK_SIZE_X + y * CHUNK_SIZE_X * CHUNK_SIZE_Z] == NULL;
				bool displayLeft = (x == 15) || _blocks[(x + 1) + z * CHUNK_SIZE_X + y * CHUNK_SIZE_X * CHUNK_SIZE_Z] == NULL;
				bool displayFront = (z == 0) || _blocks[x + (z - 1) * CHUNK_SIZE_X + y * CHUNK_SIZE_X * CHUNK_SIZE_Z] == NULL;
				bool displayBack = (z == 15) || _blocks[x + (z + 1) * CHUNK_SIZE_X + y * CHUNK_SIZE_X * CHUNK_SIZE_Z] == NULL;
				state |= (displayUp << 0);
				state |= (displayDown << 1);
				state |= (displayLeft << 2);
				state |= (displayRight << 3);
				state |= (displayFront << 4);
				state |= (displayBack << 5);
				_blocks[index]->updateNeighbors(state);
			}
		}
	}
}

void Chunk::renderBoundaries() const
{
	glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_DEPTH_TEST);
	// Cyan blue color
	glColor3f(0.0f, 0.0f, 0.0f);
	float chunkSize = 16;
	float startX = _position.x * chunkSize;
	float startZ = _position.y * chunkSize;
    float endX = startX + chunkSize;
    float endZ = startZ + chunkSize;

	glBegin(GL_LINES);

	// Vertical edges (up and down lines at corners)
	// Bottom-left corner
	glVertex3f(startX, 0.0f, startZ);
	glVertex3f(startX, 255.0f, startZ);

	// Bottom-right corner
	glVertex3f(endX, 0.0f, startZ);
	glVertex3f(endX, 255.0f, startZ);

	// Top-right corner
	glVertex3f(endX, 0.0f, endZ);
	glVertex3f(endX, 255.0f, endZ);

	// Top-left corner
	glVertex3f(startX, 0.0f, endZ);
	glVertex3f(startX, 255.0f, endZ);

	glEnd();
	glColor3d(1.0f, 1.0f, 1.0f);
}

void Chunk::freeChunkData()
{
	for (int i = 0; i < CHUNK_SIZE; i++)
	{
		if (!_blocks[i])
			continue ;
		delete _blocks[i];
		_blocks[i] = nullptr;
	}
}

void Chunk::display()
{
	for (int i = 0; i < CHUNK_SIZE; ++i)
	{
		if (_blocks[i])
		{
			_blocks[i]->display();
		}
	}
}

Chunk::~Chunk()
{
}

vec2 Chunk::getPosition(void)
{
	return _position;
}
