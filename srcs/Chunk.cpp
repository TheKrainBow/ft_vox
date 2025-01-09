#include "Chunk.hpp"

Chunk::Chunk(int chunkX, int chunkZ, NoiseGenerator &noise_gen, BiomeGenerator &biome_gen)
{
	_position = vec2(chunkX, chunkZ);
	bzero(_blocks, CHUNK_SIZE * sizeof(ABlock *));

	double relativePosX = _position.x * 16.0;
	double relativePosZ = _position.y * 16.0;

	NoiseGenerator warpingGenerator(42);
	BiomeType type;
	// Generate terrain. (Must be refactored using Perlin Noise)
	for (int x = 0; x < 16; x++)
	{
		for (int z = 0; z < 16; z++)
		{
			double scaledX = (relativePosX + x);
			double scaledZ = (relativePosZ + z);

			double offSetX = noise_gen.noise(scaledX, scaledZ);
			double offSetZ = noise_gen.noise(scaledX, scaledZ);
			double remappedX = (int)(((offSetX + 1.0) * 0.5) * 255.0);
			double remappedZ = (int)(((offSetZ + 1.0) * 0.5) * 255.0);
			type = biome_gen.findClosestBiomes(scaledX + remappedX, scaledZ + remappedZ);

			double noise = noise_gen.noise(scaledX, scaledZ);
			double remappedNoise = (int)(((noise + 1.0) * 0.5) * 25.0);
			size_t maxHeight = (size_t)(remappedNoise);
			for (size_t y = 0; y < maxHeight / 2; y++)
				_blocks[x + (z * CHUNK_SIZE_X) + (y * CHUNK_SIZE_X * CHUNK_SIZE_Z)] = new Stone((chunkX * CHUNK_SIZE_X) + x, y, (chunkZ * CHUNK_SIZE_Z) + z);
			for (size_t y = maxHeight / 2; y < maxHeight; y++)
				_blocks[x + (z * CHUNK_SIZE_X) + (y * CHUNK_SIZE_X * CHUNK_SIZE_Z)] = new Dirt((chunkX * CHUNK_SIZE_X) + x, y, (chunkZ * CHUNK_SIZE_Z) + z);
			if (type == DEFAULT || type == PLAINS)
				_blocks[x + (z * CHUNK_SIZE_X) + (maxHeight * CHUNK_SIZE_X * CHUNK_SIZE_Z)] = new Grass((chunkX * CHUNK_SIZE_X) + x, maxHeight, (chunkZ * CHUNK_SIZE_Z) + z);
			else if (type == DESERT)
				_blocks[x + (z * CHUNK_SIZE_X) + (maxHeight * CHUNK_SIZE_X * CHUNK_SIZE_Z)] = new Sand((chunkX * CHUNK_SIZE_X) + x, maxHeight, (chunkZ * CHUNK_SIZE_Z) + z);

		}
	}
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
	// Cyan blue color
	glColor3f(0.0f, 0.5f, 1.0f);
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
