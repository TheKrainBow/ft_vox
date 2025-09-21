#version 430 core

in vec3 TexCoords;
out vec4 FragColor;

uniform samplerCube skybox;

void main()
{
	vec3 dir = normalize(TexCoords);
	FragColor = texture(skybox, dir);
}
