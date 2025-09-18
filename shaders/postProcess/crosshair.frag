#version 430 core
in vec2 texCoords;
out vec4 FragColor;

uniform sampler2D screenTexture;
uniform vec2 texelSize;

void main()
{
	vec3 color = texture(screenTexture, texCoords).rgb;

	// Framebuffer size
	vec2 res = 1.0 / texelSize;
	float W = res.x;
	float H = res.y;

	// Pixel coordinates with integer grid origin at (0,0) pixel corner.
	vec2 p = gl_FragCoord.xy - vec2(0.5);

	// Exact screen center in pixel units
	float cx = (W - 1.0) * 0.5;
	float cy = (H - 1.0) * 0.5;

	// Distance (in pixels) from center
	float dx = abs(p.x - cx);
	float dy = abs(p.y - cy);

	// Crosshair dimensions (in pixels)
	const float THICKNESS = 2.0;  // total thickness (≈2 px)
	const float ARM       = 10.0; // half-length of each arm

	float halfWidth = THICKNESS * 0.5;

	bool vertical   = (dx <= halfWidth) && (dy <= ARM);
	bool horizontal = (dy <= halfWidth) && (dx <= ARM);

	// Original inversion colored crosshair
	if (vertical || horizontal)
		color = vec3(1.0) - color;

	FragColor = vec4(color, 1.0);
}
