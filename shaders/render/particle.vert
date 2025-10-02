#version 430 core

layout(location = 0) in vec2 aQuad;            // base quad [-0.5..0.5]^2
layout(location = 1) in vec3 iPos;             // world position
layout(location = 2) in float iSize;           // meters
layout(location = 3) in vec3 iColor;
layout(location = 4) in float iAlpha;          // 0..1 from remaining life
layout(location = 5) in int   iKind;           // 0 generic, 1 water

uniform mat4 projection;
uniform mat4 viewRot;     // rotation-only view matrix
uniform vec3 cameraPos;  // world-space

out vec4 vColor;         // rgba
flat out int vKind;

void main()
{
    // Camera-facing billboard in world-space
    vec3 right = vec3(viewRot[0][0], viewRot[1][0], viewRot[2][0]);
    vec3 up    = vec3(viewRot[0][1], viewRot[1][1], viewRot[2][1]);

    vec3 world = iPos + (right * aQuad.x + up * aQuad.y) * iSize;
    vec3 rel   = world - cameraPos;
    gl_Position = projection * viewRot * vec4(rel, 1.0);

    // Fade a bit near end of life
    float alpha = clamp(iAlpha, 0.0, 1.0);
    vColor = vec4(iColor, alpha);
    vKind  = iKind;
}
