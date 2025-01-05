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

using namespace glm;

enum BlockType {
	DIRT,
	COBBLE,
	STONE,
	GRASS,
	AIR,
};

#define W_WIDTH 800
#define W_HEIGHT 600
#define RENDER_DISTANCE 12

bool isWSL();
