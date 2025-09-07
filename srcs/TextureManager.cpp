#include "TextureManager.hpp"

// Load the PPM texture file
unsigned char* loadTexturePPM(const std::string& filename, int& width, int& height) {
	std::ifstream file(filename, std::ios::binary);
	if (!file) {
		std::cerr << "Failed to open PPM file: " << filename << std::endl;
		return nullptr;
	}

	std::string header;
	int maxColor;
	file >> header >> width >> height >> maxColor;
	file.ignore(1); // Skip the single whitespace

	if (header != "P6" || maxColor != 255) {
		std::cerr << "Invalid PPM format (Only P6 with 255 max color supported)." << std::endl;
		return nullptr;
	}

	size_t imageSize = (width) * (height) * 3;
	unsigned char* data = new unsigned char[imageSize];
	file.read(reinterpret_cast<char*>(data), imageSize);
	file.close();

	size_t rgbaSize = width * height * 4;  // RGBA has 4 channels
	unsigned char* rgbaData = new unsigned char[rgbaSize];

	for (size_t i = 0, j = 0; i < imageSize; i += 3, j += 4) {
		rgbaData[j] = data[i];       // Red
		rgbaData[j + 1] = data[i + 1]; // Green
		rgbaData[j + 2] = data[i + 2]; // Blue
		rgbaData[j + 3] = 255;           // Alpha (fully opaque)
	}

	delete[] data;

	return rgbaData;
}

void TextureManager::loadTexturesArray(std::vector<std::pair<TextureType, std::string>> data) {
	glGenTextures(1, &_textureArrayID);
	glBindTexture(GL_TEXTURE_2D_ARRAY, _textureArrayID);

	// Set texture parameters
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameterf(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_LOD_BIAS, 0.0f);

	// Set Anisotropic Filtering (only if supported)
	GLfloat maxAniso = 0.0f;
	glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maxAniso);
	if (maxAniso > 1.0f) {
		glTexParameterf(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAX_ANISOTROPY_EXT, maxAniso);
	}

	// Allocate storage for the texture array
	int width = TEXTURE_SIZE, height = TEXTURE_SIZE;
	glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8, width, height, N_TEXTURES, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

	// Load and upload textures
	for (int i = 0; i < N_TEXTURES; i++) {
		if (!data.empty()) {
			unsigned char *tex = loadTexturePPM(data[i].second, width, height);
			glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, i, width, height, 1, GL_RGBA, GL_UNSIGNED_BYTE, tex);
			delete [] tex;
		}
	}

	// Generate mipmaps after all textures are uploaded
	glGenerateMipmap(GL_TEXTURE_2D_ARRAY);

	glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
}

GLuint TextureManager::getTextureArray() const
{
	return _textureArrayID;
}

TextureManager::TextureManager() {
}

TextureManager::~TextureManager() {
	if (_textureArrayID) {
		glDeleteTextures(1, &_textureArrayID);
		_textureArrayID = 0;
	}
}
