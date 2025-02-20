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
#include <unistd.h>

using namespace glm;

enum Direction {
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
	WATER
};

bool isWSL();
GLuint compileShader(const char* filePath, GLenum shaderType);
GLuint createShaderProgram(const char* vertexShaderPath, const char* fragmentShaderPath);

bool isTransparentBlock(char c);

struct pair_hash {
	template <class T1, class T2>
	std::size_t operator () (const std::pair<T1, T2>& pair) const {
		auto h1 = std::hash<T1>{}(pair.first);
		auto h2 = std::hash<T2>{}(pair.second);
		return h1 ^ (h2 << 1);
	}
};
