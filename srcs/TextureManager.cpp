#include "TextureManager.hpp"

// Load the PPM texture file
t_rgb *TextureManager::loadPPM(const std::string &path, int &width, int &height) {
	std::ifstream file(path, std::ios::binary);
	if (!file.is_open()) {
		std::cerr << "Error: Couldn't open texture file " << path << std::endl;
		return nullptr;
	}
	std::string magicNumber;
	file >> magicNumber;
	if (magicNumber != "P6") {
		std::cerr << "Error: Unsupported PPM format " << magicNumber << std::endl;
		return nullptr;
	}

	file >> width >> height;
	int maxColor;
	file >> maxColor;
	file.get();  // Eat the newline character after maxColor

	// Check if the color depth is 8 bits per channel
	if (maxColor != 255) {
		std::cerr << "Error: Unsupported max color value " << maxColor << std::endl;
		return nullptr;
	}

	// Allocate memory for the Color data (one Color struct per pixel)
	t_rgb* data = new t_rgb[width * height];
		
	if (!data) {
		std::cerr << "Couldn't allocate memory for texture parsing" << std::endl;
		return nullptr;
	}
	// Read the pixel data into the array of Color structs
	file.read(reinterpret_cast<char*>(data), 3 * width * height);  // 3 bytes per pixel (Color)

	if (!file) {
		std::cerr << "Error: Couldn't read texture data from " << path << std::endl;
		delete[] data;
		return nullptr;
	}

	return data;
}

// Load texture from the file
GLuint TextureManager::loadTexture(const std::string& filename) {
	GLuint newTextureID;
	int width, height;
	t_rgb* data = loadPPM(filename, width, height);
	if (!data) {
		std::cerr << "Error: Couldn't load texture from " << filename << std::endl;
		return -1;
	}

	glGenTextures(1, &newTextureID);
	glBindTexture(GL_TEXTURE_2D, newTextureID);

	std::vector<t_rgb> rgb_data(data, data + (width * height));
	std::reverse(rgb_data.begin(), rgb_data.end());

	// Set texture parameters
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		
	// Upload the pixel data to the texture
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, rgb_data.data());
	glBindTexture(GL_TEXTURE_2D, 0);

	// Free the RGB data after uploading
	delete[] data;
	return newTextureID;
}

// Add a texture to the manager
void TextureManager::addTexture(TextureType type, std::string path) {
	_textures[type] = loadTexture(path);
}

// Retrieve a texture from the manager
GLuint TextureManager::getTexture(TextureType type) {
	return (_textures[type]);
}

// Constructor definition (no specific code yet)
TextureManager::TextureManager() {
}

// Destructor definition (no specific code yet)
TextureManager::~TextureManager() {
	for (std::map<TextureType, GLuint>::iterator it = _textures.begin(); it != _textures.end(); it++)
	{
		glDeleteTextures(1, &(it->second));
	}
}
