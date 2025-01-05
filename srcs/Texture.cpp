#include "Texture.hpp"

Texture::Texture(GLuint texture) : _texture(texture)
{
}

Texture::~Texture()
{
	resetVertex();
	glDeleteTextures(1, &_texture);
}

void Texture::addVertex(int x, int y, int z)
{
	_faces.push_back(new vec3(x, y, z));
}

void Texture::resetVertex(void)
{
	for (vec3 *face : _faces)
		delete face;
	_faces.clear();
}

void Texture::display(void)
{
	//if (_texture == 1)
	//	std::cout << _faces.size() << std::endl;
	if (_faces.size() % 4 != 0)
	{
		std::cerr << "Couldn't display because array is not divided in 4 vertex" << std::endl;
		return ;
	}
	glBegin(GL_QUADS);
	std::vector<vec3 *>::iterator it = _faces.begin();
	while (it != _faces.end())
	{
		glTexCoord2f(0, 0);
		glVertex3d((*it)->x, (*it)->y, (*it)->z);
		it++;
		glTexCoord2f(1, 0);
		glVertex3d((*it)->x, (*it)->y, (*it)->z);
		it++;
		glTexCoord2f(1, 1);
		glVertex3d((*it)->x, (*it)->y, (*it)->z);
		it++;
		glTexCoord2f(0, 1);
		glVertex3d((*it)->x, (*it)->y, (*it)->z);
		it++;
	}
	glEnd();
}

GLuint Texture::getTexture(void)
{
	return _texture;
}