#version 330 core

// Inputs
layout(location = 0) in int vertex;   // Local position (integer)

// Outputs
out vec2 uv;
out vec4 color;

// Uniforms
uniform sampler2D textureAtlas; // The texture atlas
uniform vec

uniform mat4 model;      // Model matrix
uniform mat4 view;       // View matrix
uniform mat4 projection; // Projection matrix
// Main function


void main() {
    // Step 1: Calculate local UVs based on face direction
    int x = (vertex >> 0) & 0x3F;   // 5 bits for x (0-63)
    int y = (vertex >> 6) & 0x3F;   // 5 bits for y (0-63)
    int z = (vertex >> 12) & 0x3F;  // 5 bits for z (0-63)
    int direction = (vertex >> 18) & 0x07;  // 3 bits for direction
    int textureID = (vertex >> 21) & 0x7F;  // 7 bits for textureID
    vec2 localUV = vec2((vertex >> 28) & 0x01, float((vertex >> 29) & 0x01) / 5 + (float(textureID) / 5));
    uv = localUV;

    // Step 3: Calculate texture atlas offset (stacked vertically)
    float texHeight = 1.0 / 5.0;  // 5 total textures stacked vertically
    float atlasOffsetY = float(textureID) * texHeight; // Vertical offset based on textureID

    // Step 4: Final UV calculation (local UV + vertical atlas offset)
    gl_Position = projection * view * model * vec4(x, y, z, 1.0);
}
