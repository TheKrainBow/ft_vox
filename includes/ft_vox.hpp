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
#include <list>
#include <future>
#include <type_traits>
#include <cmath>
#include <memory>
#include <tuple>

using namespace glm;

enum Direction {
	NORTH,
	SOUTH,
	WEST,
	EAST,
	DOWN,
	UP,
};

struct TopBlock {
	int height = 0;
	char type = 0;
	ivec2 pos = {0, 0};
};

typedef  struct {
	uint  count;
	uint  instanceCount;
	uint  first;
	uint  baseInstance;
} DrawArraysIndirectCommand;

struct DisplayData {
	std::vector<vec4>                       ssboData;
	std::vector<int>                        vertexData;
	std::vector<DrawArraysIndirectCommand>  indirectBufferData;
	// Per-draw metadata (one uint per DrawArraysIndirectCommand).
	// bit 0..2: face direction (0..5)
	// other bits reserved
	std::vector<uint32_t>                   drawMeta;
};

const float rectangleVertices[] =
{
	// Coords	   // texCoords
	 1.0f, -1.0f,  1.0f, 0.0f,
	-1.0f, -1.0f,  0.0f, 0.0f,
	-1.0f,  1.0f,  0.0f, 1.0f,

	 1.0f,  1.0f,  1.0f, 1.0f,
	 1.0f, -1.0f,  1.0f, 0.0f,
	-1.0f,  1.0f,  0.0f, 1.0f
};

typedef char BlockType;

bool isWSL();
GLuint compileShader(const char* filePath, GLenum shaderType);
GLuint createShaderProgram(const char* vertexShaderPath, const char* fragmentShaderPath);
bool faceDisplayCondition(char blockToDisplay, char neighborBlock, Direction dir);
GLuint compileComputeShader(const char* src);
// static glm::mat4 makeObliqueProjection(const glm::mat4& proj,
// 	const glm::mat4& view,
// 	const glm::vec4& planeWorld);

struct ivec2_hash {
	std::size_t operator () (const ivec2 vec) const {
		auto h1 = std::hash<int>{}(vec.x);
		auto h2 = std::hash<int>{}(vec.y);
		return h1 ^ (h2 << 1);
	}
};
