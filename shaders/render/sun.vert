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
	// Scale the sun with FOV so zooming in makes it appear larger (constant angular size feel).
	// Keep the same apparent size at a reference FOV (80 deg), and scale with current P[1][1].
	const float baseFovDeg = 80.0;           // reference FOV used previously
	const float baseSizeNDC = 0.15;          // vertical half-size at base FOV
	float sizeY = baseSizeNDC * tan(radians(baseFovDeg * 0.5)) * projection[1][1];

	vec2 offset = aPos * sizeY;

	// Correct for aspect ratio so the disc stays circular in pixels
	// invAspect = height / width = projection[0][0] / projection[1][1]
	float invAspect = projection[0][0] / projection[1][1];
	offset.x *= invAspect;

	// Apply offset directly in clip space (ignores camera rotation)
	gl_Position = clipPos + vec4(offset * clipPos.w, 0.0, 0.0);

	UV = aPos * 0.5 + 0.5;
}
