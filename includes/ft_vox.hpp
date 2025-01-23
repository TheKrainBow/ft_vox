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
	NORTH,
	SOUTH,
	WEST,
	EAST,
	DOWN,
	UP,
};

enum BlockType {
	AIR,
	DIRT,
	COBBLE,
	STONE,
	GRASS,
};

bool isWSL();
