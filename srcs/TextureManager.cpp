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

    return rgbaData;  // Don't forget to delete[] after usage!
}

void TextureManager::loadTexturesArray(std::vector<std::pair<TextureType, std::string>> textureList) {
	glGenTextures(1, &_textureArrayID);
	glBindTexture(GL_TEXTURE_2D_ARRAY, _textureArrayID);

	// Set texture parameters
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_REPEAT);

	int width = TEXTURE_SIZE, height = TEXTURE_SIZE;
	glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8, TEXTURE_SIZE, TEXTURE_SIZE, N_TEXTURES, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

	// Load and upload textures
	for (int i = 0; i < N_TEXTURES; i++) {
		unsigned char *data = loadTexturePPM(textureList[i].second, width, height);
		glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, i, width, height, 1, GL_RGBA, GL_UNSIGNED_BYTE, data);
		delete []data;
	}
	glGenerateMipmap(GL_TEXTURE_2D_ARRAY);

	unsigned char* debugData = new unsigned char[TEXTURE_SIZE * TEXTURE_SIZE * 5];
	glGetTexImage(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8, GL_UNSIGNED_BYTE, debugData);
	if (debugData[0] == 0 && debugData[1] == 0 && debugData[2] == 0) {
		std::cout << "Texture data is empty!" << std::endl;
	}
	for (int i = 0; i < TEXTURE_SIZE * TEXTURE_SIZE * 5; i++)
		std::cout << (char)debugData[i] << std::endl;
	delete[] debugData;
	glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
}

void TextureManager::loadTextures(std::vector<std::pair<TextureType, std::string>> textureList)
{
	(void)textureList;
	// if (textureList.empty())
	// 	return ;
	// int totalWidth = 0;
	// int totalHeight = 0;
	// std::deque<t_rgba *> textureData;
	// std::deque<int> widths;
	// std::deque<int> heights;

	// for (auto &pair : textureList)
	// {
	// 	int width;
	// 	int height;
	// 	t_rgba* rgb_data = loadPPM(pair.second, width, height);
	// 	if (!rgb_data) {
	// 		std::cerr << "Error: Skipping texture" << pair.second << std::endl;
	// 		continue ;
	// 	}
	// 	textureData.push_front(rgb_data);
	// 	widths.push_front(width);
	// 	heights.push_front(height);
	// 	totalWidth = std::max(totalWidth, width);
	// 	totalHeight += height;
	// }

	// if (textureData.empty())
	// {
	// 	std::cerr << "Error no valid textures loaded" << std::endl;
	// 	return ;
	// }


	// // Allocate memory for the combined texture
	// t_rgba* combinedData = new t_rgba[totalWidth * totalHeight];

	// // Initialize to black
	// std::fill(combinedData, combinedData + (totalWidth * totalHeight), t_rgba{0, 0, 0, 1});

	// // Combine textures vertically
	// int currentOffset = 0;
	// for (size_t i = 0; i < textureData.size(); ++i)
	// {
	// 	for (int y = 0; y < heights[i]; ++y)
	// 	{
	// 		for (int x = 0; x < widths[i]; ++x)
	// 		{
	// 			combinedData[(currentOffset + y) * totalWidth + x] = textureData[i][y * widths[i] + x];
	// 		}
	// 	}
	// 	currentOffset += heights[i];
	// 	// Free the original texture data
	// 	delete[] textureData[i];
	// }

	// std::vector<t_rgba> rgb_data(combinedData, combinedData + (totalWidth * totalHeight));
	// std::reverse(rgb_data.begin(), rgb_data.end());
	// glGenTextures(1, &_mergedTextureID);
	// glActiveTexture(GL_TEXTURE0);
	// glBindTexture(GL_TEXTURE_2D, _mergedTextureID);

	// //std::reverse(rgb_data.begin(), rgb_data.end());
	// // Upload the pixel data to the texture
	// glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, totalWidth, totalHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, rgb_data.data());

	// // Set texture parameters
	// if (isWSL())
	// 	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	// else
	// {
	// 	glGenerateMipmap(GL_TEXTURE_2D);
	// 	GLfloat maxAniso = 0.0f;
	// 	glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maxAniso);
	// 	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, maxAniso);
	// 	glTexEnvf(GL_TEXTURE_FILTER_CONTROL, GL_TEXTURE_LOD_BIAS, -25.0f);
	// 	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
	// }
	// glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	// glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	// glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    // // Clean up combined data
    // delete[] combinedData;
}

GLuint TextureManager::getMergedText() const
{
	return _mergedTextureID;
}

GLuint TextureManager::getTextureArray() const
{
	return _textureArrayID;
}

// Add a texture to the manager
void TextureManager::loadTexture(TextureType type, std::string path) {
	(void)type;
	(void)path;
	// GLuint newTextureID;
	// int width, height;
	// t_rgba* data = loadPPM(path, width, height);
	// if (!data) {
	// 	std::cerr << "Error: Couldn't load texture from " << path << std::endl;
	// 	return ;
	// }

	// glGenTextures(1, &newTextureID);
	// glActiveTexture(GL_TEXTURE0);
	// glBindTexture(GL_TEXTURE_2D, newTextureID);

	// std::vector<t_rgba> rgb_data(data, data + (width * height));
	// std::reverse(rgb_data.begin(), rgb_data.end());
	// // Upload the pixel data to the texture
	// glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, rgb_data.data());

	// // Set texture parameters
	// if (isWSL())
	// 	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	// else
	// {
	// 	glGenerateMipmap(GL_TEXTURE_2D);
	// 	GLfloat maxAniso = 0.0f;
	// 	glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maxAniso);
	// 	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, maxAniso);
	// 	glTexEnvf(GL_TEXTURE_FILTER_CONTROL, GL_TEXTURE_LOD_BIAS, -0.5f);
	// 	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
	// }
	// glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	// glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	// glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	// // Free the RGB data after uploading
	// delete[] data;
	// _textures[type] = new Texture(newTextureID);
}

// Constructor definition (no specific code yet)
TextureManager::TextureManager() {
}

// Destructor definition (no specific code yet)
TextureManager::~TextureManager() {
	for (int i = 0; i < N_TEXTURES; i++)
		delete _textures[i];
}

void TextureManager::addTextureVertex(TextureType type, Direction dir, int x, int y, int z)
{
	_textures[type]->addVertex(dir, x, y, z);
}

void TextureManager::resetTextureVertex()
{
	for (int i = 0; i < N_TEXTURES; i++)
		_textures[i]->resetVertex();
}

void TextureManager::processTextureVertex()
{
	for (int i = 0; i < N_TEXTURES; i++)
	{
		_textures[i]->processVertex(UP);
		_textures[i]->processVertex(DOWN);
		_textures[i]->processVertex(NORTH);
		_textures[i]->processVertex(SOUTH);
		_textures[i]->processVertex(EAST);
		_textures[i]->processVertex(WEST);
	}
}

int TextureManager::displayAllTexture(Camera &cam)
{
	int triangleDrown = 0;
	(void)cam;
	for (int i = 0; i < N_TEXTURES; i++)
	{
		glBindTexture(GL_TEXTURE_2D, _textures[i]->getTexture());
		triangleDrown += _textures[i]->display(UP);
		triangleDrown += _textures[i]->display(DOWN);
		triangleDrown += _textures[i]->display(SOUTH);
		triangleDrown += _textures[i]->display(NORTH);
		triangleDrown += _textures[i]->display(EAST);
		triangleDrown += _textures[i]->display(WEST);
	}
	return (triangleDrown);
}

GLuint TextureManager::getTexture(TextureType text)
{
	return _textures[text]->getTexture();
}