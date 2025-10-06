# pragma once
# define SHOW_TRIANGLES false
# define ENABLE_WORLD_GENERATION true
# define SHOW_DEBUG true
# define SHOW_LOADCHUNK_TIME true
# define RUNNING false
# define CAPTURE_MOUSE true
# define SHOW_LIGHTING true
# define SHOW_UI true
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
// Fixed window size when leaving fullscreen
#ifndef WINDOWED_FIXED_W
# define WINDOWED_FIXED_W 1280
#endif
#ifndef WINDOWED_FIXED_H
# define WINDOWED_FIXED_H 800
#endif

// Minimum time (ms) to keep the loading splash visible after startup
#ifndef LOADING_SPLASH_MS
# define LOADING_SPLASH_MS 10
#endif
# define RENDER_DISTANCE 30
# define NB_CHUNKS RENDER_DISTANCE * RENDER_DISTANCE
# define CHUNK_SIZE 32
# define SUBCHUNK_MARGIN_UP   (2 * CHUNK_SIZE)
# define SUBCHUNK_MARGIN_DOWN (0)
# define LOD_THRESHOLD 16
# define ROTATION_SPEED 1.0f
# define CACHE_SIZE NB_CHUNKS * 2
#ifndef EXTRA_CACHE_CHUNKS
// Number of chunks allowed in addition to the visible grid
// (RenderDistance*RenderDistance). Eviction will try to keep the
// farthest non-modified, non-displayed chunks pruned beyond this slack.
# define EXTRA_CACHE_CHUNKS 100
#endif
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
# define CACTUS 'k'

// Decorative plant blocks (non-solid, cutout-rendered)
# define FLOWER_POPPY        'f'
# define FLOWER_DANDELION    'g'
# define FLOWER_CYAN         'h'
# define FLOWER_SHORT_GRASS  'i'
# define FLOWER_DEAD_BUSH    'j'


// Update this number when adding new blocks (debug textbox importance)
#define NB_BLOCKS 17
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
		leaf,
		flower_poppy,
		flower_dandelion,
		flower_cyan,
		flower_short_grass,
		flower_dead_bush,
		cactus
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
	{leaf, LEAF},
	{flower_poppy,       FLOWER_POPPY},
	{flower_dandelion,   FLOWER_DANDELION},
	{flower_cyan,        FLOWER_CYAN},
	{flower_short_grass, FLOWER_SHORT_GRASS},
	{flower_dead_bush,   FLOWER_DEAD_BUSH},
	{cactus,             CACTUS}
};
// AND this block tab in Textbox.hpp line 33
// const block_pair blockTab[NB_BLOCKS]
