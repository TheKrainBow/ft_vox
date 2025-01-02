#pragma once

#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/type_ptr.hpp" 
#include "GL/glew.h"

#include <GL/glut.h>
#include <GL/freeglut.h>
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
#include <cmath>
#include <algorithm>

using namespace glm;

enum BlockType {
	DIRT,
	COBBLE,
	AIR
};

#define W_WIDTH 800
#define W_HEIGHT 600
#define RENDER_DISTANCE 11