#include "ft_vox.hpp"

GLuint compileShader(const char* filePath, GLenum shaderType)
{
	std::ifstream shaderFile(filePath);
	if (!shaderFile.is_open()) {
		std::cerr << "Error: Shader file could not be opened: " << filePath << std::endl;
		return 0;
	}

	std::stringstream shaderStream;
	shaderStream << shaderFile.rdbuf();
	std::string shaderCode = shaderStream.str();
	const char* shaderSource = shaderCode.c_str();

	GLuint shader = glCreateShader(shaderType);
	glShaderSource(shader, 1, &shaderSource, NULL);
	glCompileShader(shader);

	GLint success;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
	if (!success) {
		char infoLog[512];
		glGetShaderInfoLog(shader, 512, NULL, infoLog);
		std::cerr << "Error: Shader compilation failed\n" << infoLog << std::endl;
	}
	return shader;
}

GLuint compileComputeShader(const char* src)
{
	std::ifstream shaderFile(src);
	if (!shaderFile.is_open()) {
		std::cerr << "Error: Shader file could not be opened: " << src << std::endl;
		return 0;
	}

	std::stringstream shaderStream;
	shaderStream << shaderFile.rdbuf();
	std::string shaderCode = shaderStream.str();
	const char* shaderSource = shaderCode.c_str();

	GLuint s = glCreateShader(GL_COMPUTE_SHADER);
	glShaderSource(s, 1, &shaderSource, nullptr);
	glCompileShader(s);
	GLint status = GL_FALSE; glGetShaderiv(s, GL_COMPILE_STATUS, &status);
	if (!status) {
		GLint len = 0; glGetShaderiv(s, GL_INFO_LOG_LENGTH, &len);
		std::string log(len, '\0');
		glGetShaderInfoLog(s, len, nullptr, log.data());
		std::cerr << "[Compute] compile error:\n" << log << std::endl;
	}
	GLuint p = glCreateProgram();
	glAttachShader(p, s);
	glLinkProgram(p);
	glDeleteShader(s);
	glGetProgramiv(p, GL_LINK_STATUS, &status);
	if (!status) {
		GLint len = 0; glGetProgramiv(p, GL_INFO_LOG_LENGTH, &len);
		std::string log(len, '\0');
		glGetProgramInfoLog(p, len, nullptr, log.data());
		std::cerr << "[Compute] link error:\n" << log << std::endl;
	}
	return p;
}

GLuint createShaderProgram(const char* vertexShaderPath, const char* fragmentShaderPath)
{
	GLuint vertexShader = compileShader(vertexShaderPath, GL_VERTEX_SHADER);
	GLuint fragmentShader = compileShader(fragmentShaderPath, GL_FRAGMENT_SHADER);

	GLuint shaderProgram = glCreateProgram();
	glAttachShader(shaderProgram, vertexShader);
	glAttachShader(shaderProgram, fragmentShader);
	glLinkProgram(shaderProgram);

	GLint success;
	glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
	if (!success) {
		char infoLog[512];
		glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
		std::cerr << "Error: Shader program linking failed\n" << infoLog << std::endl;
	}

	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);
	return shaderProgram;
}
