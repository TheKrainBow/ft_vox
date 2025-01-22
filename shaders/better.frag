#version 330 core

// Inputs from the vertex shader
// in vec2 finalUV;          // The final UV coordinates calculated in the vertex shader
in vec2 uv;          // The final UV coordinates calculated in the vertex shader
in vec4 color;          // The final UV coordinates calculated in the vertex shader

// Output color
out vec4 FragColor;       // The output color of the fragment

// Uniforms
uniform sampler2D textureAtlas;  // The texture atlas

void main() {
    // Sample the texture atlas using the calculated UV coordinates
    // FragColor = vec4(finalUV.x, finalUV.y, 0.0, 1.0);
    // FragColor = vec4(1.0, 0.0, 0.0, 1.0); 
    FragColor = color;
    FragColor = texture(textureAtlas, uv);
}