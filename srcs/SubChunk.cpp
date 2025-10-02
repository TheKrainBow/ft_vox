#include "SubChunk.hpp"

SubChunk::SubChunk(ivec3 position,
    PerlinMap *perlinMap,
    CaveGenerator &caveGen,
    Chunk &chunk,
    ChunkLoader &chunkLoader,
    int resolution)
: _chunkLoader(chunkLoader), _chunk(chunk), _caveGen(caveGen)
{
    _position = position;
    _resolution = resolution;
    _heightMap = &perlinMap->heightMap;
    _treeMap = &perlinMap->treeMap;
    _biomeMap = &perlinMap->biomeMap;
    _grassMap = &perlinMap->grassMap;
    _flowerMask = &perlinMap->flowerMask;
    _flowerR = &perlinMap->flowerR;
    _flowerY = &perlinMap->flowerY;
    _flowerB = &perlinMap->flowerB;
	_chunkSize = CHUNK_SIZE / resolution;
	size_t size = _chunkSize * _chunkSize * _chunkSize;
	_blocks = std::make_unique<uint8_t[]>(size);
	std::fill_n(_blocks.get(), size, 0);
	_memorySize = sizeof(*this) + size;
	_isFullyLoaded = false;
}

size_t SubChunk::getMemorySize() {
	return _memorySize;
}

void SubChunk::loadHeight(int prevResolution)
{
	(void)prevResolution;

	for (int z = 0; z < CHUNK_SIZE ; z += _resolution)
	{
		for (int x = 0; x < CHUNK_SIZE ; x += _resolution)
		{
			int maxHeight = (*_heightMap)[z * CHUNK_SIZE + x];
			for (int y = 0; y < CHUNK_SIZE ; y += _resolution)
			{
				int globalY = y + _position.y * CHUNK_SIZE;
				if (globalY <= 0) {
					if (globalY == 0)
						setBlock(x, y, z, BEDROCK);
					continue;
				}
				// Default: solid below surface
				if (globalY > maxHeight)
					break;
				
				if (!CAVES || _resolution != 1 || !_caveGen.isAir(
						x + _position.x * CHUNK_SIZE,
						globalY,
						z + _position.z * CHUNK_SIZE,
						maxHeight + 40
					))
					setBlock(x, y, z, STONE);
			}
		}
	}
}

void SubChunk::loadOcean(int x, int z, size_t ground, size_t adjustOceanHeight)
{
	int y;

	// Fill water from sea level down to ground
	for (y = adjustOceanHeight; y > (int)ground; y--)
		setBlock(x, y - _position.y * CHUNK_SIZE, z, WATER);

	// Sea floor composition: thinner sand near shore, thicker slightly deeper
	// Depth measured in blocks below sea level
	int depth = (int)adjustOceanHeight - (int)ground;
	int sandLayers = std::clamp(depth / 3, 1, 2); // 1 for very shallow, 2 for moderate

	// Place a sand cap at ground and a few layers beneath
	setBlock(x, ((int)ground) - _position.y * CHUNK_SIZE, z, SAND);
	for (int i = 1; i <= sandLayers; ++i)
		setBlock(x, ((int)ground - i) - _position.y * CHUNK_SIZE, z, SAND);

	// Transition to dirt below sand
	for (int i = sandLayers + 1; i <= sandLayers + 4; ++i)
		setBlock(x, ((int)ground - i) - _position.y * CHUNK_SIZE, z, DIRT);
}

void SubChunk::plantTree(int x, int y, int z, double /*proba*/) {
	// vertical unit step in WORLD voxels
	const int trunkH = 10;
	const int totalH = 13;

	// World-space coordinates of the planting base
	const int baseWX = x + _position.x * CHUNK_SIZE;
	const int baseWY = y + _position.y * CHUNK_SIZE;
	const int baseWZ = z + _position.z * CHUNK_SIZE;

	auto chunkOf = [](int wx, int wz) -> ivec2 {
		return ivec2((int)std::floor((double)wx / (double)CHUNK_SIZE),
						(int)std::floor((double)wz / (double)CHUNK_SIZE));
	};
	auto isAir = [&](int wx, int wy, int wz) -> bool {
		ivec2 cpos = chunkOf(wx, wz);
		return _chunkLoader.getBlock(cpos, ivec3(wx, wy, wz)) == AIR;
	};

	// Ensure space is clear for trunk column and canopy envelope before planting
	for (int i = 0; i <= totalH; ++i) {
		int wy = baseWY + i;
		if (!isAir(baseWX, wy, baseWZ)) return; // trunk collision

		if (i >= 4 && i <= 10) {
			if (!isAir(baseWX + 1, wy, baseWZ)) return;
			if (!isAir(baseWX - 1, wy, baseWZ)) return;
			if (!isAir(baseWX, wy, baseWZ + 1)) return;
			if (!isAir(baseWX, wy, baseWZ - 1)) return;
		}
		if (i >= 5 && i <= 7) {
			if (!isAir(baseWX + 2, wy, baseWZ)) return;
			if (!isAir(baseWX - 2, wy, baseWZ)) return;
			if (!isAir(baseWX, wy, baseWZ + 2)) return;
			if (!isAir(baseWX, wy, baseWZ - 2)) return;
			if (!isAir(baseWX + 1, wy, baseWZ + 1)) return;
			if (!isAir(baseWX - 1, wy, baseWZ + 1)) return;
			if (!isAir(baseWX + 1, wy, baseWZ - 1)) return;
			if (!isAir(baseWX - 1, wy, baseWZ - 1)) return;
		}
	}

	// Place trunk and leaves
	for (int i = 0; i <= totalH; ++i) {
		int yy = y + i;                 // step = 1, not _resolution
		if (i <= trunkH) setBlock(x, yy, z, LOG);
		else             setBlock(x, yy, z, LEAF);

		if (i >= 4 && i <= 10) {
			setBlock(x + 1, yy, z, LEAF);
			setBlock(x - 1, yy, z, LEAF);
			setBlock(x, yy, z + 1, LEAF);
			setBlock(x, yy, z - 1, LEAF);
		}
		if (i >= 5 && i <= 7) {
			setBlock(x + 2, yy, z, LEAF);
			setBlock(x - 2, yy, z, LEAF);
			setBlock(x, yy, z + 2, LEAF);
			setBlock(x, yy, z - 2, LEAF);
			setBlock(x + 1, yy, z + 1, LEAF);
			setBlock(x - 1, yy, z + 1, LEAF);
			setBlock(x + 1, yy, z - 1, LEAF);
			setBlock(x - 1, yy, z - 1, LEAF);
		}
	}
}

void SubChunk::loadPlaine(int x, int z, size_t ground)
{
    // --- ground shaping ---
    int y = ground - _position.y * CHUNK_SIZE;

    for (int i = 1; i <= 4; i++)
        if (getBlock({x, y - (i * _resolution), z}) != AIR)
            setBlock(x, y - (i * _resolution), z, DIRT);

    if (getBlock({x, y, z}) == AIR)
		return ;
	setBlock(x, y, z, GRASS);

    // --- deterministic RNG helpers (stable across reloads) ---
    auto splitmix64 = [](uint64_t v) {
        v += 0x9e3779b97f4a7c15ULL;
        v = (v ^ (v >> 30)) * 0xbf58476d1ce4e5b9ULL;
        v = (v ^ (v >> 27)) * 0x94d049bb133111ebULL;
        return v ^ (v >> 31);
    };
    auto hash2 = [&](int WX, int WZ, uint64_t salt){
        uint64_t k = (uint64_t)((uint32_t)WX) << 32 | (uint32_t)WZ;
        k ^= salt * 0x9e3779b97f4a7c15ULL;
        return splitmix64(k);
    };
    auto rand01 = [&](int WX, int WZ, uint64_t salt){
        // Convert 53 high bits to double in [0,1)
        return (hash2(WX, WZ, salt) >> 11) * (1.0 / 9007199254740992.0);
    };


    // --- world coords (for persistent RNG) ---
    const int WX = x + _position.x * CHUNK_SIZE;
    const int WZ = z + _position.z * CHUNK_SIZE;

    // --- sample maps (optional; guard for nullptrs) ---
    int idx = z * CHUNK_SIZE + x;

    double g  = 0.5; // fallback if maps missing
    double f  = 0.0;
    double rC = 0.33, yC = 0.33, bC = 0.34;

    if (_grassMap && *_grassMap)           g  = (*_grassMap)[idx];
    if (_flowerMask && *_flowerMask)       f  = (*_flowerMask)[idx];
    if (_flowerR && *_flowerR)             rC = (*_flowerR)[idx];
    if (_flowerY && *_flowerY)             yC = (*_flowerY)[idx];
    if (_flowerB && *_flowerB)             bC = (*_flowerB)[idx];

    // --- deterministic RNG draws ---
    double j0 = rand01(WX, WZ, 1); // grass gate
    double j1 = rand01(WX, WZ, 2); // flower gate in cluster
    double j2 = rand01(WX, WZ, 3); // flower color pick

    // --- biome weighting: default density for generic plains/forest behavior ---
    double biomeGrass = 0.6;

	// Deterministic Bernoulli for grass (no Perlin patchiness),
    // slightly modulated by g to add tiny variation.
    double grassProb = biomeGrass + 0.2 * (g - 0.5);
    if (grassProb < 0.0) grassProb = 0.0;
    if (grassProb > 1.0) grassProb = 1.0;
    bool spawnGrass = (j0 < grassProb);

    // Flower clusters: loosen threshold + small gate inside cluster
    bool inFlowerArea = (f > 0.4);
    bool spawnFlower  = inFlowerArea && (j1 < 0.08); // ~8% inside clusters

    // --- place plant above ground if air ---
    if (getBlock({x, y+1, z}) != AIR)
        return;

    if (spawnFlower) {
        // weighted color pick
        double sum = rC + yC + bC + 1e-6;
        double pr = rC / sum, py = yC / sum; // pb = rest
        char ftype = (j2 < pr) ? FLOWER_POPPY
                  : (j2 < pr + py) ? FLOWER_DANDELION
                                   : FLOWER_CYAN;
        setBlock(x, y + 1, z, ftype);
    } else if (spawnGrass) {
        setBlock(x, y + 1, z, FLOWER_SHORT_GRASS);
    }
}

// Plains variant: only rare short grass, no flowers
void SubChunk::loadPlainsRare(int x, int z, size_t ground)
{
    // Ground shaping identical to loadPlaine
    int y = ground - _position.y * CHUNK_SIZE;
    for (int i = 1; i <= 4; i++)
        if (getBlock({x, y - (i * _resolution), z}) != AIR)
            setBlock(x, y - (i * _resolution), z, DIRT);
    if (getBlock({x, y, z}) == AIR)
        return;
    setBlock(x, y, z, GRASS);

    // Deterministic RNG
    auto splitmix64 = [](uint64_t v) {
        v += 0x9e3779b97f4a7c15ULL;
        v = (v ^ (v >> 30)) * 0xbf58476d1ce4e5b9ULL;
        v = (v ^ (v >> 27)) * 0x94d049bb133111ebULL;
        return v ^ (v >> 31);
    };
    auto hash2 = [&](int WX, int WZ, uint64_t salt){
        uint64_t k = (uint64_t)((uint32_t)WX) << 32 | (uint32_t)WZ;
        k ^= salt * 0x9e3779b97f4a7c15ULL;
        return splitmix64(k);
    };
    auto rand01 = [&](int WX, int WZ, uint64_t salt){
        return (hash2(WX, WZ, salt) >> 11) * (1.0 / 9007199254740992.0);
    };

    const int WX = x + _position.x * CHUNK_SIZE;
    const int WZ = z + _position.z * CHUNK_SIZE;
    int idx = z * CHUNK_SIZE + x;
    double g  = 0.5;
    if (_grassMap && *_grassMap) g = (*_grassMap)[idx];

    // Only rare short grass, modulated very slightly by g
    double j0 = rand01(WX, WZ, 11);
    double grassProb = 0.02 + 0.02 * (g - 0.5); // ~2% baseline
    if (grassProb < 0.0) grassProb = 0.0;
    if (grassProb > 1.0) grassProb = 1.0;

    // place plant above ground if air
    if (getBlock({x, y+1, z}) != AIR)
        return;
    if (j0 < grassProb)
        setBlock(x, y + 1, z, FLOWER_SHORT_GRASS);
}

// Plains with clustered patches of grass/flowers + sparse singles
void SubChunk::loadPlainsPatches(int x, int z, size_t ground)
{
    // --- ground shaping (same as plains) ---
    int y = ground - _position.y * CHUNK_SIZE;
    for (int i = 1; i <= 4; i++)
        if (getBlock({x, y - (i * _resolution), z}) != AIR)
            setBlock(x, y - (i * _resolution), z, DIRT);
    if (getBlock({x, y, z}) == AIR)
        return;
    setBlock(x, y, z, GRASS);

    // Deterministic RNG helpers
    auto splitmix64 = [](uint64_t v) {
        v += 0x9e3779b97f4a7c15ULL;
        v = (v ^ (v >> 30)) * 0xbf58476d1ce4e5b9ULL;
        v = (v ^ (v >> 27)) * 0x94d049bb133111ebULL;
        return v ^ (v >> 31);
    };
    auto hash2 = [&](int WX, int WZ, uint64_t salt){
        uint64_t k = (uint64_t)((uint32_t)WX) << 32 | (uint32_t)WZ;
        k ^= salt * 0x9e3779b97f4a7c15ULL;
        return splitmix64(k);
    };
    auto rand01 = [&](int WX, int WZ, uint64_t salt){
        return (hash2(WX, WZ, salt) >> 11) * (1.0 / 9007199254740992.0);
    };

    // World coordinates and map samples
    const int WX = x + _position.x * CHUNK_SIZE;
    const int WZ = z + _position.z * CHUNK_SIZE;
    const int idx = z * CHUNK_SIZE + x;

    double g  = 0.5; // grass noise (fine scale)
    double f  = 0.0; // low-frequency flower/patch mask
    double rC = 0.33, yC = 0.33, bC = 0.34; // flower color weights
    if (_grassMap && *_grassMap)     g  = (*_grassMap)[idx];
    if (_flowerMask && *_flowerMask) f  = (*_flowerMask)[idx];
    if (_flowerR && *_flowerR)       rC = (*_flowerR)[idx];
    if (_flowerY && *_flowerY)       yC = (*_flowerY)[idx];
    if (_flowerB && *_flowerB)       bC = (*_flowerB)[idx];

    // Only place if above is empty
    if (getBlock({x, y + 1, z}) != AIR)
        return;

    // --- Cluster centers and neighborhood membership (organic blobs) ---
    // Select sparse centers, then compute a distance-weighted influence with jittered radii.
    // This produces soft, rounded patches instead of rigid 3x3 squares.
    double baseCenter = 0.010;                         // ~1.0% base
    double centerProb = baseCenter + 0.010 * std::max(0.0, f - 0.5) + 0.004 * (g - 0.5);
    centerProb = std::clamp(centerProb, 0.002, 0.030);

    double patchScore = 0.0;
    // Wider search window to allow larger patches
    for (int dz = -6; dz <= 6; ++dz)
    for (int dx = -6; dx <= 6; ++dx)
    {
        double sC = rand01(WX + dx, WZ + dz, 401);
        if (sC >= centerProb) continue; // not a center

        // Randomized radius per center (in blocks)
        double R = 2.4 + 4.8 * rand01(WX + dx, WZ + dz, 402); // ~2.4 .. 7.2 (bigger max)
        double d = std::sqrt(double(dx*dx + dz*dz));
        if (d > R + 0.75) continue; // quick reject
        double k = std::max(0.0, 1.0 - (d / std::max(0.001, R)));

        // Small anisotropic jitter to break symmetry
        double jitter = 0.85 + 0.30 * rand01(WX + dx * 7, WZ + dz * 13, 406); // 0.85..1.15
        k *= jitter;

        if (k > patchScore) patchScore = k;
    }

    // Edge threshold with local jitter and slight bias from f
    double tEdge = 0.50;
    tEdge -= 0.10 * std::clamp(f - 0.5, -0.5, 0.5); // flowery areas slightly expand patches
    tEdge += 0.08 * rand01(WX, WZ, 405);
    bool inPatch = (patchScore > tEdge);

    // --- Singles probabilities (kept low to preserve patch feel) ---
    double jG = rand01(WX, WZ, 11);
    double jF = rand01(WX, WZ, 12);
    double jT = rand01(WX, WZ, 13); // type pick in patch

    // Singles: light sprinkling
    double singleGrassP  = 0.020 + 0.015 * (g - 0.5);                 // ~2% avg
    double singleFlowerP = (0.006 + 0.014 * std::max(0.0, f - 0.3));  // ~0.6–2%
    singleGrassP  = std::clamp(singleGrassP,  0.005, 0.05);
    singleFlowerP = std::clamp(singleFlowerP, 0.002, 0.03);

    // Normalize flower color weights
    double sumC = rC + yC + bC + 1e-6;
    double pr = rC / sumC, py = yC / sumC; // pb = 1 - pr - py

    auto placeFlower = [&](double pick){
        char ftype = (pick < pr) ? FLOWER_POPPY
                    : (pick < pr + py) ? FLOWER_DANDELION
                                       : FLOWER_CYAN;
        setBlock(x, y + 1, z, ftype);
    };

    if (inPatch) {
        // Inside a patch: mostly grass with some flowers, and a little chance of holes
        // Bias with f so flowery regions get more flowers
        double flowerBias = 0.20 + 0.35 * std::clamp(f - 0.4, 0.0, 0.6); // ~20–41%
        double holeP = 0.10; // small gaps to avoid carpets
        if (jT < holeP) return; // leave empty
        if (jT < holeP + flowerBias) {
            placeFlower(rand01(WX, WZ, 502));
        } else {
            setBlock(x, y + 1, z, FLOWER_SHORT_GRASS);
        }
    } else {
        // Sparse singles outside patches
        if (jF < singleFlowerP) {
            placeFlower(rand01(WX, WZ, 503));
        } else if (jG < singleGrassP) {
            setBlock(x, y + 1, z, FLOWER_SHORT_GRASS);
        }
    }
}

void SubChunk::loadMountain(int x, int z, size_t ground)
{
	int y = ground - _position.y * CHUNK_SIZE;

	if (getBlock({x, y, z}) != AIR)
		setBlock(x, y, z, SNOW);
	for (int i = 1; i <= 5; i++)
		if (getBlock({x, y - (i * _resolution), z}) != AIR)
			setBlock(x, y - (i * _resolution), z, SNOW);
}

void SubChunk::loadDesert(int x, int z, size_t ground)
{
    int y = ground - _position.y * CHUNK_SIZE;

    setBlock(x, y, z, SAND);
    for (int i = 1; i <= 4; i++)
        setBlock(x, y - (i * _resolution), z, SAND);

    // Decorations: cactus columns and rare dead bushes
    // Deterministic RNG
    auto splitmix64 = [](uint64_t v) {
        v += 0x9e3779b97f4a7c15ULL;
        v = (v ^ (v >> 30)) * 0xbf58476d1ce4e5b9ULL;
        v = (v ^ (v >> 27)) * 0x94d049bb133111ebULL;
        return v ^ (v >> 31);
    };
    auto hash2 = [&](int WX, int WZ, uint64_t salt){
        uint64_t k = (uint64_t)((uint32_t)WX) << 32 | (uint32_t)WZ;
        k ^= salt * 0x9e3779b97f4a7c15ULL;
        return splitmix64(k);
    };
    auto rand01 = [&](int WX, int WZ, uint64_t salt){
        return (hash2(WX, WZ, salt) >> 11) * (1.0 / 9007199254740992.0);
    };

    const int WX = x + _position.x * CHUNK_SIZE;
    const int WZ = z + _position.z * CHUNK_SIZE;

    if (getBlock({x, y+1, z}) != AIR)
        return;

    double rCac = rand01(WX, WZ, 101);
    double rBush = rand01(WX, WZ, 102);

    // Try cactus first (sparser)
    if (rCac < 0.006) {
        int h = 2 + (int)std::floor(rand01(WX, WZ, 103) * 3.0); // 2..4
        // Ensure vertical clearance
        for (int i = 1; i <= h; ++i) {
            if (y + i >= CHUNK_SIZE) { h = i - 1; break; }
            if (getBlock({x, y + i, z}) != AIR) { h = i - 1; break; }
        }
        if (h >= 1) {
            for (int i = 1; i <= h; ++i) setBlock(x, y + i, z, CACTUS);
            return;
        }
    }

    // Otherwise, small chance for dead bush
    if (rBush < 0.012) {
        setBlock(x, y + 1, z, FLOWER_DEAD_BUSH);
    }
}

void SubChunk::loadBeach(int x, int z, size_t ground)
{
	// Nudge beaches up to meet sea level to avoid lower-than-water beaches
	int beachTopWorldY = std::max((int)ground, (int)OCEAN_HEIGHT);

	// Fill from existing ground up to beachTopWorldY with sand
	for (int wy = (int)ground; wy <= beachTopWorldY; ++wy)
		setBlock(x, wy - _position.y * CHUNK_SIZE, z, SAND);

	// Add a little thickness below
	for (int i = 1; i <= 2; ++i)
		setBlock(x, (beachTopWorldY - i) - _position.y * CHUNK_SIZE, z, SAND);
}

void SubChunk::loadSnowy(int x, int z, size_t ground)
{
	int y = ground - _position.y * CHUNK_SIZE;

	// Snow on top, frozen soil below
	if (getBlock({x, y, z}) != AIR)
		setBlock(x, y, z, SNOW);
	for (int i = 1; i <= 5; i++)
		if (getBlock({x, y - (i * _resolution), z}) != AIR)
			setBlock(x, y - (i * _resolution), z, SNOW);
}

void SubChunk::loadForest(int x, int z, size_t ground)
{
	// Base like plains
	loadPlaine(x, z, ground);

	// Denser than plains, but enforce spacing via local maxima
	const int groundSnap = (int)ground - ((int)ground % _resolution);
	const int yLocal = groundSnap - _position.y * CHUNK_SIZE;
	if (yLocal < 0 || yLocal >= CHUNK_SIZE) return;

	if (getBlock({x, yLocal, z}) != GRASS) return;

	const double treeP = (*_treeMap)[z * CHUNK_SIZE + x];
	if (treeP <= 0.64) return; // slightly denser than before

	const int radius = 4;
	const double* tmap = *_treeMap;
	for (int dz = -radius; dz <= radius; ++dz) {
		int nz = z + dz;
		if (nz < 0 || nz >= CHUNK_SIZE) continue;
		for (int dx = -radius; dx <= radius; ++dx) {
			int nx = x + dx;
			if (nx < 0 || nx >= CHUNK_SIZE) continue;
			if (dx == 0 && dz == 0) continue;
			if (tmap[nz * CHUNK_SIZE + nx] > treeP) return; // not a local maximum
		}
	}

	plantTree(x, yLocal + 1, z, treeP);
}

void SubChunk::loadTree(int x, int z)
{
	const int ground = (int)(*_heightMap)[z * CHUNK_SIZE + x];

	// snap to the same quantization used during terrain write
	const int groundSnap = ground - (ground % _resolution);

	// local to this subchunk
	const int yLocal = groundSnap - _position.y * CHUNK_SIZE;
	if (yLocal < 0 || yLocal >= CHUNK_SIZE) return ;

	// only plant on grass
	if (getBlock({x, yLocal, z}) != GRASS) return ;

	const double treeP = (*_treeMap)[z * CHUNK_SIZE + x];
	if (treeP <= 0.85) return ;

	// Poisson-like spacing: only plant if local maximum within radius
	const int radius = 4;
	const double* tmap = *_treeMap;
	for (int dz = -radius; dz <= radius; ++dz) {
		int nz = z + dz;
		if (nz < 0 || nz >= CHUNK_SIZE) continue;
		for (int dx = -radius; dx <= radius; ++dx) {
			int nx = x + dx;
			if (nx < 0 || nx >= CHUNK_SIZE) continue;
			if (dx == 0 && dz == 0) continue;
			if (tmap[nz * CHUNK_SIZE + nx] > treeP) return; // not a local maximum
		}
	}

	// base just above groundSnap
	plantTree(x, yLocal + 1, z, treeP);
}

void SubChunk::markLoaded(bool loaded) {
	_isFullyLoaded = loaded;
}


void SubChunk::loadBiome(int prevResolution)
{
	(void)prevResolution;
	for (int x = 0; x < CHUNK_SIZE ; x += _resolution)
	{
		for (int z = 0; z < CHUNK_SIZE ; z += _resolution)
		{
			Biome biome = (*_biomeMap)[z * CHUNK_SIZE + x];
			double surfaceLevel = (*_heightMap)[z * CHUNK_SIZE + x];
			surfaceLevel = surfaceLevel - (int(surfaceLevel) % _resolution);
			int adjustOceanHeight = OCEAN_HEIGHT - (OCEAN_HEIGHT % _resolution);
			switch (biome)
			{
				case PLAINS:
					// Plains: clustered patches of grass/flowers + sparse singles
					loadPlainsPatches(x, z, surfaceLevel);
					break ;
				case DESERT:
					loadDesert(x, z, surfaceLevel);
					break;
				case MOUNTAINS:
					loadMountain(x, z, surfaceLevel);
					break;
				case SNOWY:
					loadSnowy(x, z, surfaceLevel);
					break;
				case FOREST:
					loadForest(x, z, surfaceLevel);
					break;
				case OCEAN:
					// Use a smaller offset to reduce visible sand walls near shores
					loadOcean(x, z, surfaceLevel + 1, adjustOceanHeight);
					break;
				case BEACH:
					loadBeach(x, z, surfaceLevel);
					break;
				default :
					break;
			}
			// Default tree planting only for non-forest grasslands
			if (biome == PLAINS)
				loadTree(x, z);
		}
	}
		_isFullyLoaded = true;
}


// void SubChunk::loadBiome(int prevResolution)
// {
// 	(void)prevResolution
// ;	NoiseGenerator &noisegen = _chunkLoader.getNoiseGenerator();
// 	noisegen.setNoiseData({
// 		1.0, 0.9, 0.02, 0.5, 12
// 	});

// 	const int adjustOceanHeight = OCEAN_HEIGHT - (OCEAN_HEIGHT % _resolution);

// 	for (int x = 0; x < CHUNK_SIZE ; x += _resolution)
// 	{
// 		for (int z = 0; z < CHUNK_SIZE ; z += _resolution)
// 		{
// 			double ground = (*_heightMap)[z * CHUNK_SIZE + x];
// 			ground = ground - (int(ground) % _resolution);
		
// 			if (ground <= OCEAN_HEIGHT)
// 				loadOcean(x, z, ground + 3, adjustOceanHeight);
// 			else if (ground >= MOUNT_HEIGHT + (noisegen.noise(x + _position.x * CHUNK_SIZE, z + _position.z * CHUNK_SIZE) * 15))
// 				loadMountain(x, z, ground);
// 			else if (ground)
// 				loadPlaine(x, z, ground);
// 		}
// 	}

// 	for (int x = 0; x < CHUNK_SIZE; ++x) {
// 		for (int z = 0; z < CHUNK_SIZE; ++z) {
// 			const int ground = (int)(*_heightMap)[z * CHUNK_SIZE + x];

// 			// snap to the same quantization used during terrain write
// 			const int groundSnap = ground - (ground % _resolution);

// 			// local to this subchunk
// 			const int yLocal = groundSnap - _position.y * CHUNK_SIZE;
// 			if (yLocal < 0 || yLocal >= CHUNK_SIZE) continue;

// 			// only plant on grass
// 			if (getBlock({x, yLocal, z}) != GRASS) continue;

// 			const double treeP = (*_treeMap)[z * CHUNK_SIZE + x];
// 			if (treeP <= 0.72) continue;

// 			// base just above groundSnap
// 			plantTree(x, yLocal + 1, z, treeP);
// 		}
// 	}
// 	_isFullyLoaded = true;
// }

SubChunk::~SubChunk()
{
	_loaded = false;
}

void SubChunk::setBlock(int x, int y, int z, char block)
{
	// x,y,z are subchunk-local voxel coords (can be outside [0..CHUNK_SIZE) to spill into neighbors)

	// Convert to absolute world voxel coordinates
	const int wx = x + _position.x * CHUNK_SIZE;
	const int wy = y + _position.y * CHUNK_SIZE;
	const int wz = z + _position.z * CHUNK_SIZE;

	// floor-div that works with negatives
	auto fdiv = [](int a, int b) -> int {
		return (int)std::floor((double)a / (double)b);
	};

	// Which chunk column / subchunk does this absolute voxel land in?
	const ivec2 targetChunk = { fdiv(wx, CHUNK_SIZE), fdiv(wz, CHUNK_SIZE) };
	const int   targetSubY  = fdiv(wy, CHUNK_SIZE);

	// Is it inside THIS subchunk?
	const bool inThisChunk = (targetChunk.x == _position.x && targetChunk.y == _position.z);
	const bool inThisSubY  = (targetSubY     == _position.y);

	if (inThisChunk && inThisSubY)
	{
		if (_chunkSize <= 0) return;

		// Write guarded to avoid races with LOD changes
		{
			std::lock_guard<std::mutex> lk(_dataMutex);
			if (_chunkSize <= 0 || !_blocks)
				return;

			// Local indices inside this subchunk’s voxel grid (in world voxel space)
			int lx = (wx % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;
			int ly = (wy % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;
			int lz = (wz % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;

			// Downscale to this subchunk’s resolution
			lx /= _resolution;
			ly /= _resolution;
			lz /= _resolution;

			if (lx >= 0 && ly >= 0 && lz >= 0 &&
				lx < _chunkSize && ly < _chunkSize && lz < _chunkSize)
			{
				const size_t plane = static_cast<size_t>(_chunkSize) * static_cast<size_t>(_chunkSize);
				const size_t idx = static_cast<size_t>(lx)
					+ static_cast<size_t>(lz) * static_cast<size_t>(_chunkSize)
					+ static_cast<size_t>(ly) * plane;
				_blocks[idx] = block;
			}
		}
		return;
	}

	// Otherwise, delegate to World using ABSOLUTE world coords
	_chunkLoader.setBlockOrQueue(targetChunk, { wx, wy, wz }, static_cast<BlockType>(block), /*byPlayer=*/false);
	_chunkLoader.markChunkDirty(targetChunk);
}

void SubChunk::setBlockLocal(int x, int y, int z, char block)
{
	// Direct write for pre-localized coordinates. Used by ChunkLoader to
	// avoid re-dispatching and recursion when the target subchunk is known.
	std::lock_guard<std::mutex> lk(_dataMutex);
	if (_chunkSize <= 0 || !_blocks)
		return;

	int lx = x / _resolution;
	int ly = y / _resolution;
	int lz = z / _resolution;
	if (lx < 0 || ly < 0 || lz < 0 || lx >= _chunkSize || ly >= _chunkSize || lz >= _chunkSize)
		return;

	const size_t plane = static_cast<size_t>(_chunkSize) * static_cast<size_t>(_chunkSize);
	const size_t idx = static_cast<size_t>(lx)
		+ static_cast<size_t>(lz) * static_cast<size_t>(_chunkSize)
		+ static_cast<size_t>(ly) * plane;
	_blocks[idx] = block;
}


char SubChunk::getBlock(ivec3 position)
{
	// Guard concurrent mutations of _blocks / _chunkSize
	std::lock_guard<std::mutex> lk(_dataMutex);

	if (_chunkSize <= 0 || !_blocks)
		return AIR;

	int x = position.x;
	int y = position.y;
	int z = position.z;

	// Convert to local cell coordinates at this subchunk's resolution
	x /= _resolution;
	y /= _resolution;
	z /= _resolution;
	if (x < 0 || y < 0 || z < 0 || x >= _chunkSize || y >= _chunkSize || z >= _chunkSize)
		return AIR;

	const size_t plane = static_cast<size_t>(_chunkSize) * static_cast<size_t>(_chunkSize);
	const size_t idx = static_cast<size_t>(x)
		+ static_cast<size_t>(z) * static_cast<size_t>(_chunkSize)
		+ static_cast<size_t>(y) * plane;
	const size_t maxSize = plane * static_cast<size_t>(_chunkSize);
	if (idx >= maxSize)
		return AIR;
	return _blocks[idx];
}

void SubChunk::addDownFace(BlockType current, ivec3 position, TextureType texture, bool isTransparent)
{
	char block = 0;
	if (position.y > 0)
		block = getBlock({position.x, position.y - _resolution, position.z});
	else
	{
		SubChunk *underChunk = nullptr;
		underChunk = _chunk.getSubChunk(_position.y - 1);
		if (!underChunk)
			block = AIR;
		else
			block = underChunk->getBlock({position.x, CHUNK_SIZE - _resolution, position.z});
	}
	if (faceDisplayCondition(current, block, DOWN))
		addFace(position, DOWN, texture, isTransparent);
}

void SubChunk::addUpFace(BlockType current, ivec3 position, TextureType texture, bool isTransparent)
{
	char block = 0;
	if (position.y != CHUNK_SIZE - _resolution)
		block = getBlock({position.x, position.y + _resolution, position.z});
	else
	{
		SubChunk *overChunk = _chunk.getSubChunk(_position.y + 1);
		if (overChunk)
			block = overChunk->getBlock({position.x, 0, position.z});
		else
			block = AIR;

	}
	if (faceDisplayCondition(current, block, UP))
		addFace(position, UP, texture, isTransparent);
}


void SubChunk::addNorthFace(BlockType current, ivec3 position, TextureType texture, bool isTransparent)
{
	if (position.z != 0) {
		if (isNeighborTransparent(ivec3(position.x, position.y, position.z - _resolution), NORTH, current, _resolution))
			return addFace(position, NORTH, texture, isTransparent);
		return;
	}
	auto chunk = _chunk.getNorthChunk();      // neighbor chunk
	// If neighbor isn't loaded yet, consider border as visible (air)
	if (!chunk) { addFace(position, NORTH, texture, isTransparent); return; }

	SubChunk* subChunk = chunk->getSubChunk(_position.y);
	if (!subChunk || !subChunk->isNeighborTransparent(ivec3(position.x, position.y, CHUNK_SIZE - subChunk->_resolution), NORTH, current, _resolution))
		return;

	addFace(position, NORTH, texture, isTransparent);
}

void SubChunk::addSouthFace(BlockType current, ivec3 position, TextureType texture, bool isTransparent)
{
	if (position.z != CHUNK_SIZE - _resolution) {
		if (isNeighborTransparent(ivec3(position.x, position.y, position.z + _resolution), SOUTH, current, _resolution))
			return addFace(position, SOUTH, texture, isTransparent);
		return;
	}
	auto chunk = _chunk.getSouthChunk();
	if (!chunk) { addFace(position, SOUTH, texture, isTransparent); return; }

	SubChunk* subChunk = chunk->getSubChunk(_position.y);
	if (!subChunk || !subChunk->isNeighborTransparent(ivec3(position.x, position.y, 0), SOUTH, current, _resolution))
		return;

	addFace(position, SOUTH, texture, isTransparent);
}

void SubChunk::addWestFace(BlockType current, ivec3 position, TextureType texture, bool isTransparent)
{
	if (position.x != 0) {
		if (isNeighborTransparent(ivec3(position.x - _resolution, position.y, position.z), WEST, current, _resolution))
			return addFace(position, WEST, texture, isTransparent);
		return;
	}
	auto chunk = _chunk.getWestChunk();
	if (!chunk) { addFace(position, WEST, texture, isTransparent); return; }

	SubChunk* subChunk = chunk->getSubChunk(_position.y);
	if (!subChunk || !subChunk->isNeighborTransparent(ivec3(CHUNK_SIZE - subChunk->_resolution, position.y, position.z), WEST, current, _resolution))
		return;

	addFace(position, WEST, texture, isTransparent);
}

void SubChunk::addEastFace(BlockType current, ivec3 position, TextureType texture, bool isTransparent)
{
	if (position.x != CHUNK_SIZE - _resolution) {
		if (isNeighborTransparent(ivec3(position.x + _resolution, position.y, position.z), EAST, current, _resolution))
			return addFace(position, EAST, texture, isTransparent);
		return;
	}
	auto chunk = _chunk.getEastChunk();
	if (!chunk) { addFace(position, EAST, texture, isTransparent); return; }

	SubChunk* subChunk = chunk->getSubChunk(_position.y);
	if (!subChunk || !subChunk->isNeighborTransparent(ivec3(0, position.y, position.z), EAST, current, _resolution))
		return;

	addFace(position, EAST, texture, isTransparent);
}

void SubChunk::addBlock(BlockType block, ivec3 position, TextureType down, TextureType up, TextureType north, TextureType south, TextureType east, TextureType west, bool isTransparent = false)
{
	addUpFace(block, position, up, isTransparent);
	addDownFace(block, position, down, isTransparent);
	addNorthFace(block, position, north, isTransparent);
	addSouthFace(block, position, south, isTransparent);
	addWestFace(block, position, west, isTransparent);
	addEastFace(block, position, east, isTransparent);
}

// Show all faces
// void SubChunk::addBlock(BlockType block, ivec3 position, TextureType down, TextureType up, TextureType north, TextureType south, TextureType east, TextureType west, bool isTransparent = false)
// {
// 	(void)block;
// 	addFace(position, NORTH, north, isTransparent);
// 	addFace(position, SOUTH, south, isTransparent);
// 	addFace(position, WEST, west, isTransparent);
// 	addFace(position, UP, up, isTransparent);
// 	addFace(position, DOWN, down, isTransparent);
// 	addFace(position, EAST, east, isTransparent);
// }

ivec3 SubChunk::getPosition()
{
	return _position;
}

void SubChunk::clearFaces() {
	// std::cout << "Cleared faces" << std::endl;
	for (int i = 0; i < 6; i++)
	{
		_faces[i].clear();
		_transparentFaces[i].clear();
	}
	_vertexData.clear();
	_transparentVertexData.clear();
	_hasSentFaces = false;
	_needUpdate = true;
	_needTransparentUpdate = true;
}

void SubChunk::updateResolution(int resolution, PerlinMap *perlinMap)
{
	// Prevent readers during rebuild
	markLoaded(false);

    int prevResolution = _resolution;
    _heightMap = &perlinMap->heightMap;
    _biomeMap  = &perlinMap->biomeMap;
    _treeMap   = &perlinMap->treeMap;

    // Prepare fresh storage for new LOD, then atomically publish shape + buffer
    int newChunkSize = CHUNK_SIZE / std::max(1, resolution);
    size_t newSize = static_cast<size_t>(newChunkSize) * static_cast<size_t>(newChunkSize) * static_cast<size_t>(newChunkSize);
    std::unique_ptr<uint8_t[]> fresh(new uint8_t[newSize]()); // zero-initialize
    {
        std::lock_guard<std::mutex> lk(_dataMutex);
        _resolution = std::max(1, resolution);
        _chunkSize = newChunkSize;
        _blocks.swap(fresh);
        _memorySize = sizeof(*this) + newSize;
    }

	// Rebuild content at the new resolution
	loadHeight(prevResolution);
	loadBiome(prevResolution);
}


void SubChunk::sendFacesToDisplay()
{
    if (!_isFullyLoaded)
        return ;
    clearFaces();

	for (int x = 0; x < CHUNK_SIZE; x += _resolution)
	{
		for (int y = 0; y < CHUNK_SIZE; y += _resolution)
		{
			for (int z = 0; z < CHUNK_SIZE; z += _resolution)
			{
				switch (getBlock({x, y, z}))
				{
					case 0:
						break;
					case DIRT:
						addBlock(DIRT, ivec3(x, y, z), T_DIRT, T_DIRT, T_DIRT, T_DIRT, T_DIRT, T_DIRT);
						break;
					case COBBLE:
						addBlock(COBBLE, ivec3(x, y, z), T_COBBLE, T_COBBLE, T_COBBLE, T_COBBLE, T_COBBLE, T_COBBLE);
						break;
					case BEDROCK:
						addBlock(BEDROCK, ivec3(x, y, z), T_BEDROCK, T_BEDROCK, T_BEDROCK, T_BEDROCK, T_BEDROCK, T_BEDROCK);
						break;
					case STONE:
						addBlock(STONE, ivec3(x, y, z), T_STONE, T_STONE, T_STONE, T_STONE, T_STONE, T_STONE);
						break;
					case GRASS:
						addBlock(GRASS, ivec3(x, y, z), T_DIRT, T_GRASS_TOP, T_GRASS_SIDE, T_GRASS_SIDE, T_GRASS_SIDE, T_GRASS_SIDE);
						break;
					case SAND:
						addBlock(SAND, ivec3(x, y, z), T_SAND, T_SAND, T_SAND, T_SAND, T_SAND, T_SAND);
						break;
					case WATER:
						addBlock(WATER, ivec3(x, y, z), T_WATER, T_WATER, T_WATER, T_WATER, T_WATER, T_WATER, true);
						break;
					case SNOW:
						addBlock(SNOW, ivec3(x, y, z), T_SNOW, T_SNOW, T_SNOW, T_SNOW, T_SNOW, T_SNOW);
						break;
					case LOG:
						addBlock(LOG, ivec3(x, y, z), T_LOG_TOP, T_LOG_TOP, T_LOG_SIDE, T_LOG_SIDE, T_LOG_SIDE, T_LOG_SIDE);
						break;
					case LEAF:
						addBlock(LEAF, ivec3(x, y, z), T_LEAF, T_LEAF, T_LEAF, T_LEAF, T_LEAF, T_LEAF, true);
						break;
					case CACTUS:
						addBlock(CACTUS, ivec3(x, y, z), T_CACTUS_TOP, T_CACTUS_TOP, T_CACTUS_SIDE, T_CACTUS_SIDE, T_CACTUS_SIDE, T_CACTUS_SIDE);
						break;
						// Route leaves to transparent pass (masked alpha). Always add all 6 faces.
						// addUpFace(   LEAF, ivec3(x, y, z), T_LEAF, true);
						// addDownFace( LEAF, ivec3(x, y, z), T_LEAF, true);
						// addNorthFace(LEAF, ivec3(x, y, z), T_LEAF, true);
						// addSouthFace(LEAF, ivec3(x, y, z), T_LEAF, true);
						// addWestFace( LEAF, ivec3(x, y, z), T_LEAF, true);
						// addEastFace( LEAF, ivec3(x, y, z), T_LEAF, true);
						// break;
					default :
						break;
				}
			}
		}
	}
    processFaces(false);
    processFaces(true);
}

void SubChunk::addTextureVertex(Face face, std::vector<int> *vertexData)
{
	int x = face.position.x;
	int y = face.position.y;
	int z = face.position.z;
	int textureID = face.texture;
	int direction = face.direction;
	if (x < 0 || y < 0 || z < 0 || x >= CHUNK_SIZE || y >= CHUNK_SIZE || z >= CHUNK_SIZE || direction >= 6)
		return ;
	int newVertex = 0;
	int lengthX = face.size.x - 1;
	int lengthY = face.size.y - 1;

	if (face.direction == EAST || face.direction == WEST)
	{
		lengthX = face.size.y - 1;
		lengthY = face.size.x - 1;
	}

	newVertex |= (x & 0x1F) << 0;			// 5 bits for x
	newVertex |= (y & 0x1F) << 5;			// 5 bits for y
	newVertex |= (z & 0x1F) << 10;			// 5 bits for z
    // Repacked (direction moved to per-draw metadata)
    // bits 15..19: lengthX (5)
    // bits 20..24: lengthY (5)
    // bits 25..31: textureID (7)
    newVertex |= (lengthX & 0x1F) << 15;
    newVertex |= (lengthY & 0x1F) << 20;
    newVertex |= (textureID & 0x7F) << 25;
	vertexData->push_back(newVertex);
}

void SubChunk::processFaces(bool isTransparent)
{
	if (isTransparent)
	{
		// Reset counts
		for (int i=0;i<6;++i) _transpDirCounts[i]=0;
		size_t before = _transparentVertexData.size();
		processUpVertex(_transparentFaces, &_transparentVertexData);
		_transpDirCounts[UP] = (int)(_transparentVertexData.size() - before);
		before = _transparentVertexData.size();
		processDownVertex(_transparentFaces, &_transparentVertexData);
		_transpDirCounts[DOWN] = (int)(_transparentVertexData.size() - before);
		before = _transparentVertexData.size();
		processNorthVertex(_transparentFaces, &_transparentVertexData);
		_transpDirCounts[NORTH] = (int)(_transparentVertexData.size() - before);
		before = _transparentVertexData.size();
		processSouthVertex(_transparentFaces, &_transparentVertexData);
		_transpDirCounts[SOUTH] = (int)(_transparentVertexData.size() - before);
		before = _transparentVertexData.size();
		processEastVertex(_transparentFaces, &_transparentVertexData);
		_transpDirCounts[EAST] = (int)(_transparentVertexData.size() - before);
		before = _transparentVertexData.size();
		processWestVertex(_transparentFaces, &_transparentVertexData);
		_transpDirCounts[WEST] = (int)(_transparentVertexData.size() - before);
	}
	else
	{
		for (int i=0;i<6;++i) _dirCounts[i]=0;
		size_t before = _vertexData.size();
		processUpVertex(_faces, &_vertexData);
		_dirCounts[UP] = (int)(_vertexData.size() - before);
		before = _vertexData.size();
		processDownVertex(_faces, &_vertexData);
		_dirCounts[DOWN] = (int)(_vertexData.size() - before);
		before = _vertexData.size();
		processNorthVertex(_faces, &_vertexData);
		_dirCounts[NORTH] = (int)(_vertexData.size() - before);
		before = _vertexData.size();
		processSouthVertex(_faces, &_vertexData);
		_dirCounts[SOUTH] = (int)(_vertexData.size() - before);
		before = _vertexData.size();
		processEastVertex(_faces, &_vertexData);
		_dirCounts[EAST] = (int)(_vertexData.size() - before);
		before = _vertexData.size();
		processWestVertex(_faces, &_vertexData);
		_dirCounts[WEST] = (int)(_vertexData.size() - before);
	}
}

std::vector<int> &SubChunk::getVertices() {
	return _vertexData;
}

std::vector<int> &SubChunk::getTransparentVertices()
{
	return _transparentVertexData;
}

# define IS_TRANSPARENT true
# define IS_SOLID false

bool SubChunk::isNeighborTransparent(ivec3 position, Direction dir, char viewerBlock, int viewerResolution) {
	if (viewerResolution == _resolution)
		return (faceDisplayCondition(viewerBlock, getBlock(position), dir));
	if (viewerResolution < _resolution)
		return (IS_SOLID);
	position /= _resolution;
	position *= _resolution;
	int res2 = _resolution * 2;
	for (int x = 0; x < res2; x += _resolution) {
		for (int y = 0; y < res2; y += _resolution) {
			for (int z = 0; z < res2; z += _resolution) {
				if (faceDisplayCondition(viewerBlock, getBlock(ivec3(position.x + x, position.y + y, position.z + z)), dir))
					return IS_TRANSPARENT;
				if (dir == NORTH || dir == SOUTH)
					break ;
				}
				if (dir == UP || dir == DOWN)
					break ;
			}
		if (dir == EAST || dir == WEST)
			break ;
	}
	return IS_SOLID;
}
