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

std::string readShaderProgram(const std::string &filePath)
{
    std::ifstream shaderFile(filePath);
    if (!shaderFile.is_open())
	{
        std::cerr << "Error: Shader file could not be opened: " << filePath << std::endl;
        return 0;
    }
    std::stringstream shaderStream;
    shaderStream << shaderFile.rdbuf();
    std::string shaderCode = shaderStream.str();
	return shaderCode;
}