#version 430 core

layout(location = 0) in vec2 aPos; // Quad corners [-1,1] x [-1,1]

uniform vec3 sunPosition;
uniform mat4 view;
uniform mat4 projection;

out vec2 UV;

void main()
{
    // Remove camera rotation from view matrix
    mat4 rotView = mat4(mat3(view)); // strip translation
    vec4 billboardCenter = projection * rotView * vec4(sunPosition, 1.0);

    // Scale billboard size in screen space
    float size = 0.15; // screen size
    vec2 offset = aPos * size * billboardCenter.w;

    // Final projected position with offset in clip space
    gl_Position = billboardCenter + vec4(offset, 0.0, 0.0);
    UV = aPos * 0.5 + 0.5;
}
