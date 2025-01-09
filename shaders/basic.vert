#version 330 core

// Attributes (per-vertex data)
layout(location = 0) in vec3 position;  // Vertex position
layout(location = 1) in vec2 texCoord; // Texture coordinates

// Outputs (passed to fragment shader)
out vec2 TexCoord; // Texture coordinates

// Uniforms (global data)
uniform mat4 model;      // Model matrix
uniform mat4 view;       // View matrix
uniform mat4 projection; // Projection matrix

void main() {
    // Apply transformations and output final position
    gl_Position = projection * view * model * vec4(position, 1.0);

    // Pass the texture coordinates to the fragment shader
    TexCoord = texCoord;
}
