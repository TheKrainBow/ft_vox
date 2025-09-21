#version 430 core

layout (location = 0) in vec3 aPos;

out vec3 TexCoords;

uniform mat4 view;       // rotation only (no translation)
uniform mat4 projection;

void main()
{
    TexCoords = aPos;
    vec4 pos = projection * view * vec4(aPos, 1.0);
    // Push depth to far plane to avoid clipping and ensure proper background
    gl_Position = vec4(pos.xy, pos.w, pos.w);
}
