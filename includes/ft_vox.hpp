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
#include <list>
#include <future>
#include <type_traits>


enum Direction {
	NORTH,
	SOUTH,
	WEST,
	EAST,
	DOWN,
	UP,
};

const float rectangleVertices[] =
{
	// Coords    // texCoords
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
// void mouseCallback(GLFWwindow* window, double x, double y);
// void keyPress(GLFWwindow* window, int key, int scancode, int action, int mods);
// void reshape(GLFWwindow* window, int width, int height);
bool faceDisplayCondition(char blockToDisplay, char neighborBlock);

struct vec2_hash {
	std::size_t operator () (const glm::vec2 vec) const {
		auto h1 = std::hash<int>{}(vec.x);
		auto h2 = std::hash<int>{}(vec.y);
		return h1 ^ (h2 << 1);
	}
};
