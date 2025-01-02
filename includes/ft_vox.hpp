#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp> 

using namespace glm;

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