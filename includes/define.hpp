# pragma once
# define SHOW_TRIANGLES false
# define ENABLE_WORLD_GENERATION true
# define SHOW_DEBUG true
# define SHOW_LOADCHUNK_TIME true
# define RUNNING false
# define CAPTURE_MOUSE true
# define SHOW_LIGHTING false
# define GRAVITY false
# define IGNORE_MOUSE false
# define KEY_INIT false

# define W_WIDTH 1000
# define W_HEIGHT 800
# define RENDER_DISTANCE 11
# define NB_CHUNKS RENDER_DISTANCE * RENDER_DISTANCE
# define CHUNK_SIZE 32
# define LOD_THRESHOLD 32
# define ROTATION_SPEED 1.0f
# define CACHE_SIZE NB_CHUNKS * 2
# define MOVEMENT_SPEED 0.5f

# define RESOLUTION 1

# define OCEAN_HEIGHT 111
# define MOUNT_HEIGHT 260

# define AIR 0
# define STONE 'S'
# define DIRT 'D'
# define GRASS 'G'
# define SAND 's'
# define WATER 'W'
# define SNOW 'w'

typedef  struct {
    uint  count;
    uint  instanceCount;
    uint  first;
    uint  baseInstance;
} DrawArraysIndirectCommand;
