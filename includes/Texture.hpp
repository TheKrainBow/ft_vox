#pragma once

#include "ft_vox.hpp"

class Texture
{
private:
	GLuint				_texture;
	std::vector<vec3 *>	_faces;
public:
	Texture(GLuint texture);
	~Texture();
	void addVertex(int x, int y, int z);
	GLuint getTexture(void);
	void resetVertex(void);
	int display(void);
};
