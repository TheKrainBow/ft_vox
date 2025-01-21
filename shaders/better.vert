#version 330 core

// Inputs
layout(location = 0) in int vertex;   // Local position (integer)

// Outputs
out vec4 color;

// Uniforms
uniform sampler2D textureAtlas; // The texture atlas

uniform mat4 model;      // Model matrix
uniform mat4 view;       // View matrix
uniform mat4 projection; // Projection matrix
// Main function

void main() {
    // Step 1: Calculate local UVs based on face direction
    int x = (vertex >> 0) & 0x3F;   // 6 bits for x (0-63)
    int y = (vertex >> 6) & 0x3F;   // 6 bits for y (0-63)
    int z = (vertex >> 12) & 0x3F;  // 6 bits for z (0-63)
    int direction = (vertex >> 18) & 0x07;  // 3 bits for direction
    int textureID = (vertex >> 21) & 0x7F;  // 7 bits for textureID
    int U = (vertex >> 28) & 0x01;  // Get the value of U (either 0 or 1)
    int V = (vertex >> 29) & 0x01; 
    gl_Position = projection * view * model * vec4(x, y, z, 1.0);

    vec2 localUV;

    // Step 2: Calculate the correct local UVs based on the face direction
    if (direction == 0 || direction == 1) { // Front/Back (Z-aligned)
        localUV = vec2(float(x), float(y)) / 31.0;  // Use XY plane (normalized)
    } else if (direction == 2 || direction == 3) { // Top/Bottom (Y-aligned)
        localUV = vec2(float(x), float(z)) / 31.0;  // Use XZ plane (normalized)
    } else if (direction == 4 || direction == 5) { // Left/Right (X-aligned)
        localUV = vec2(float(y), float(z)) / 31.0;  // Use YZ plane (normalized)
    }

    // Step 3: Calculate texture atlas offset (stacked vertically)
    float texHeight = 1.0 / 5.0;  // 5 total textures stacked vertically
    float atlasOffsetY = float(textureID) * texHeight; // Vertical offset based on textureID

    // Step 4: Final UV calculation (local UV + vertical atlas offset)
    vec2 finalUV = vec2(localUV.x, localUV.y + atlasOffsetY); // Adjust Y to sample from the texture

    // Debugging U and V: Just output them as colors for debugging
    // Use (U, V) to show debugging values
    // color = vec4(float(U), float(V), 0.0, 1.0);

    // Step 5: Sample the texture atlas and set the fragment color
    color = texture(textureAtlas, finalUV);
}