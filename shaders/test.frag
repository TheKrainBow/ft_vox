#version 330 core

// Inputs from the vertex shader
// in vec2 finalUV;					// The final UV coordinates calculated in the vertex shader
in vec2 TexCoord;							// The final UV coordinates calculated in the vertex shader
//in vec4 color;						// The final UV coordinates calculated in the vertex shader

// Output color
out vec4 FragColor;       // The output color of the fragment

// Uniforms
uniform sampler2D textureAtlas;  // The texture atlas

void main() {
    FragColor = texture(textureAtlas, TexCoord);
}