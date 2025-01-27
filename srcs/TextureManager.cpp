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

// Add a texture to the manager
void TextureManager::loadTexture(TextureType type, std::string path) {
	GLuint newTextureID;
	int width, height;
	t_rgb* data = loadPPM(path, width, height);
	if (!data) {
		std::cerr << "Error: Couldn't load texture from " << path << std::endl;
		return ;
	}

	glGenTextures(1, &newTextureID);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, newTextureID);

	std::vector<t_rgb> rgb_data(data, data + (width * height));
	std::reverse(rgb_data.begin(), rgb_data.end());
	// Upload the pixel data to the texture
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, rgb_data.data());

	// Set texture parameters
	if (isWSL())
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	else
	{
		glGenerateMipmap(GL_TEXTURE_2D);
		GLfloat maxAniso = 0.0f;
		glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maxAniso);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, maxAniso);
		glTexEnvf(GL_TEXTURE_FILTER_CONTROL, GL_TEXTURE_LOD_BIAS, -0.5f);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
	}
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	// Free the RGB data after uploading
	delete[] data;
	_textures[type] = new Texture(newTextureID);
}

// Constructor definition (no specific code yet)
TextureManager::TextureManager() {
}

// Destructor definition (no specific code yet)
TextureManager::~TextureManager() {
	for (int i = 0; i < N_TEXTURES; i++)
		delete _textures[i];
}

void TextureManager::addTextureVertex(TextureType type, e_direction dir, int x, int y, int z)
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