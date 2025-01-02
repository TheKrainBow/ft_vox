#pragma once

#include "glm.hpp"
#include "gtc/matrix_transform.hpp"
#include "gtc/type_ptr.hpp" 

using namespace glm;

#include <GL/glew.h>
#include <GL/glut.h>
#include <GL/freeglut.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <map>
#include <string>
#include <array>
#include <limits>
#include <memory>
#include <cmath>
#include <algorithm>


enum BlockType {
	DIRT,
	COBBLE,
	AIR
};

#define W_WIDTH 800
#define W_HEIGHT 600
#define RENDER_DISTANCE 11