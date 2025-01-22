#version 330 core

// Inputs
layout(location = 0) in int vertex;   // Local position (integer)

// Outputs
out vec2 uv;
out vec4 color;

// Uniforms
uniform sampler2D textureAtlas; // The texture atlas

uniform mat4 model;      // Model matrix
uniform mat4 view;       // View matrix
uniform mat4 projection; // Projection matrix
// Main function


vec2 getVertex(int index) {
    // Extract the 2 bits for the given index (0-3)
    int shift = index * 2;
    int bits = (packedVertices >> shift) & 0b11;

    // Convert the 2 bits into (x, y) coordinates
    return vec2(float(bits & 0b01), float((bits >> 1) & 0b01));
}

void main() {
    // Step 1: Calculate local UVs based on face direction
    int x = (vertex >> 0) & 0x3F;   // 6 bits for x (0-63)
    int y = (vertex >> 6) & 0x3F;   // 6 bits for y (0-63)
    int z = (vertex >> 12) & 0x3F;  // 6 bits for z (0-63)
    int direction = (vertex >> 18) & 0x07;  // 3 bits for direction
    int textureID = (vertex >> 21) & 0x7F;  // 7 bits for textureID
    //vec3 normal = NORMALS[direction];
    vec2 localUV = vec2((vertex >> 28) & 0x01, float((vertex >> 29) & 0x01) / 5 + (float(textureID) / 5));
    uv = localUV;

    // Step 3: Calculate texture atlas offset (stacked vertically)
    float texHeight = 1.0 / 5.0;  // 5 total textures stacked vertically
    float atlasOffsetY = float(textureID) * texHeight; // Vertical offset based on textureID

    // Step 4: Final UV calculation (local UV + vertical atlas offset)
    //uv = vec2(float((vertex >> 28) & 0x01), float((vertex >> 29) & 0x01) / 5); // Adjust Y to sample from the texture
    //uv = vec2(localUV.x, float(localUV.y) * texHeight + atlasOffsetY);
    gl_Position = projection * view * model * vec4(x, y, z, 1.0);
}
