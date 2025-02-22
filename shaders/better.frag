#version 430 core

uniform sampler2DArray textureArray;
in vec2 TexCoord;							// The final UV coordinates calculated in the vertex shader
flat in int TextureID;

// Output color
out vec4 FragColor;       // The output color of the fragment

void main() {
    // FragColor = vec4(0, TexCoord.x / 5, 0, 1);
    // FragColor = vec4(TexCoord, 0, 1);
    FragColor = texture(textureArray, vec3(TexCoord, TextureID));
    // FragColor = vec4(0, 0, 0, 1);
}