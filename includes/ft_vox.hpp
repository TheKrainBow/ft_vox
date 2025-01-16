#pragma once

#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/type_ptr.hpp" 
#include "GL/glew.h"

#include <GLFW/glfw3.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <unordered_set>
#include <map>
#include <string>
#include <array>
#include <limits>
#include <memory>
#include <random>
#include <algorithm>
#include <iomanip>
#include <queue>
#include <mutex>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <stdexcept>

using namespace glm;

enum e_direction {
	UP,
	DOWN,
	NORTH,
	SOUTH,
	WEST,
	EAST
};

enum BlockType {
	AIR,
	DIRT,
	COBBLE,
	STONE,
	GRASS,
};

#define W_WIDTH 800
#define W_HEIGHT 600
#define RENDER_DISTANCE 10
#define NB_CHUNKS RENDER_DISTANCE * RENDER_DISTANCE
#define Y_RENDER_DISTANCE 3
#define XZ_RENDER_DISTANCE 10


bool isWSL();
