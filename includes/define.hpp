# pragma once
# define SHOW_TRIANGLES false
# define ENABLE_WORLD_GENERATION true
# define SHOW_DEBUG true
# define SHOW_LOADCHUNK_TIME true
# define RUNNING false
# define CAPTURE_MOUSE true
# define SHOW_LIGHTING false
# define GRAVITY false
# define FALLING false
# define SWIMMING false
# define JUMPING false
# define UNDERWATER false
# define ASCENDING false
# define IGNORE_MOUSE false
# define KEY_INIT false

# define W_WIDTH 1000
# define W_HEIGHT 800
# define RENDER_DISTANCE 31
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

# define PLAYER_HEIGHT 1.8f
# define EYE_HEIGHT 1.62f
# define EPS 0.02f

# define KHR_DEBUG false
# define CAVES false

# define RESOLUTION 1

# define OCEAN_HEIGHT 111
# define MOUNT_HEIGHT 260

# define SCHOOL_SAMPLES 1

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

typedef  struct {
	uint  count;
	uint  instanceCount;
	uint  first;
	uint  baseInstance;
} DrawArraysIndirectCommand;
