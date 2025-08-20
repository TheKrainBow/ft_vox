#version 430 core

uniform sampler2D screenTexture;
uniform vec3 sunPos;
uniform mat4 view;
uniform mat4 projection;
uniform vec2 screenSize;

in vec2 texCoords;
out vec4 FragColor;

void main()
{
	// === Tunables ===
	const float exposure	= 0.35;
	const float decay		= 0.92;
	const float density		= 0.3;	// Much shorter rays
	const float weight		= 0.03;	// Slightly stronger
	const int numSamples	= 48;	// Less streaky, faster
	// ================

	// Project sun world position to screen space
	vec4 sunClip = projection * view * vec4(sunPos, 1.0);
	sunClip /= sunClip.w;
	vec2 sunScreen = sunClip.xy * 0.5 + 0.5;

	vec2 fragCoord = texCoords;
	vec2 deltaTexCoord = fragCoord - sunScreen;
	deltaTexCoord *= density / float(numSamples);

	vec2 coord = fragCoord;
	float illuminationDecay = 1.0;
	vec3 color = vec3(0.0);

	for (int i = 0; i < numSamples; ++i) {
		coord -= deltaTexCoord;
		vec3 sampleColor = texture(screenTexture, coord).rgb;
		sampleColor *= illuminationDecay * weight;
		color += sampleColor;
		illuminationDecay *= decay;
	}

	color *= exposure;
	FragColor = vec4(color, 1.0);
}
