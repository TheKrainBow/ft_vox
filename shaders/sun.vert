#version 430 core

layout (location = 0) in vec3 aPos;  // Vertex position
out vec2 TexCoord;  // Output texture coordinates

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform vec3 sunPos;  // Sun's position based on time

void main()
{
    // Use the sun's position to place the sun in the world space
    vec4 worldPos = view * model * vec4(aPos + sunPos, 1.0f);  // Translate by sunPos

    // Apply the projection matrix
    gl_Position = projection * worldPos;
    TexCoord = aPos.xy;  // Pass texture coordinates (optional)
}
