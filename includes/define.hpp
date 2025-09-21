# pragma once
# define SHOW_TRIANGLES false
# define ENABLE_WORLD_GENERATION true
# define SHOW_DEBUG true
# define SHOW_LOADCHUNK_TIME true
# define RUNNING false
# define CAPTURE_MOUSE true
# define SHOW_LIGHTING false
# define GRAVITY true
# define FALLING false
# define SWIMMING false
# define JUMPING false
# define UNDERWATER false
# define ASCENDING false
# define SPRINTING false
# define IGNORE_MOUSE false
# define KEY_INIT false

# define W_WIDTH 1000
# define W_HEIGHT 800
# define RENDER_DISTANCE 120
# define NB_CHUNKS RENDER_DISTANCE * RENDER_DISTANCE
# define CHUNK_SIZE 32
# define SUBCHUNK_MARGIN_UP   (2 * CHUNK_SIZE)
# define SUBCHUNK_MARGIN_DOWN (0)
# define LOD_THRESHOLD 32
# define ROTATION_SPEED 1.0f
# define CACHE_SIZE NB_CHUNKS * 2
# define MOVEMENT_SPEED 0.5f
# define FALL_INCREMENT 9.8f / 40.0f
# define FALL_INCREMENT_WATER 9.8f / 1000.0f

// --- Simulation helpers (convert old per-tick gravity to per-second) ---
#define TICK_RATE 20.0f
#define GRAVITY_PER_SEC (FALL_INCREMENT * TICK_RATE)
#define FALL_DAMP_PER_TICK 0.98f

# define PLAYER_HEIGHT 1.8f
# define EYE_HEIGHT 1.62f
# define EPS 0.02f

# define KHR_DEBUG false
# define CAVES true

# define RESOLUTION 1

# define OCEAN_HEIGHT 111
# define MOUNT_HEIGHT 260

# define SCHOOL_SAMPLES 1

// Camera frustum planes
#ifndef NEAR_PLANE
# define NEAR_PLANE 0.1f
#endif
#ifndef FAR_PLANE
# define FAR_PLANE 9600.0f
#endif

// Single-file cubemap PNG (cross/strip/grid). If present, used first.
#ifndef SKYBOX_SINGLE_PNG
# define SKYBOX_SINGLE_PNG "textures/cloud1.png"
#endif

# define AIR 0
# define STONE 'S'
# define COBBLE 'C'
# define BEDROCK 'B'
# define DIRT 'D'
# define GRASS 'G'
# define SAND 's'
# define WATER 'W'
# define SNOW 'w'
# define LOG 'l'
# define LEAF 'L'

// Update this number when adding new blocks (debug textbox importance)
#define NB_BLOCKS 11
// AND the enum list (pls)
enum block_types {
	air,
	stone,
	cobble,
	bedrock,
	dirt,
	grass,
	sand,
	water,
	snow,
	oak_log,
	leaf
};
struct block_correspondance
{
	block_types block;
	char correspondance;
};
// AND the correspondance chart (pls v2)
const block_correspondance blockDebugTab[NB_BLOCKS] = {
	{air, AIR},
	{stone, STONE},
	{cobble, COBBLE},
	{bedrock, BEDROCK},
	{dirt, DIRT},
	{grass, GRASS},
	{sand, SAND},
	{water, WATER},
	{snow, SNOW},
	{oak_log, LOG},	// ambiguous name so had to specify
	{leaf, LEAF}
};

typedef  struct {
	uint  count;
	uint  instanceCount;
	uint  first;
	uint  baseInstance;
} DrawArraysIndirectCommand;
