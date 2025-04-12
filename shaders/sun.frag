#version 430 core

out vec4 FragColor;
in vec2 TexCoord;  // Texture coordinates

uniform vec3 sunColor;  // Color of the sun

void main()
{
    // Simple shading: the sun's color
    FragColor = vec4(sunColor, 1.0f);
}
