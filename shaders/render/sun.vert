#version 430 core

layout(location = 0) in vec2 aPos; // Quad corners [-1,1] x [-1,1]

uniform vec3 sunPosition;
uniform mat4 view;
uniform mat4 projection;

out vec2 UV;

void main()
{
	// World → View → Clip
	vec4 viewPos  = view * vec4(sunPosition, 1.0);
	vec4 clipPos  = projection * viewPos;

	// Billboard in NDC (always aligned to screen, independent of camera rotation)
	float size = 0.15;
	vec2 offset = aPos * size;

	// Apply offset directly in clip space (ignores camera rotation)
	gl_Position = clipPos + vec4(offset * clipPos.w, 0.0, 0.0);

	UV = aPos * 0.5 + 0.5;
}