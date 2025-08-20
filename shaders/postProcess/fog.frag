#version 430 core

out vec4 FragColor;
in vec2 texCoords;

uniform sampler2D screenTexture;
uniform sampler2D depthTexture;
uniform float renderDistance;

void main()
{
	vec3 currentColor = texture(screenTexture, texCoords).rgb;
	float currentDepth = texture(depthTexture, texCoords).r;
	vec3 finalColor = currentColor;

	vec3 fogColor = vec3(0.53, 0.81, 0.92);

	// Scale fog with render distance
	float fogStart = renderDistance * 5.0;
	float fogEnd   = renderDistance * 15.0;

	float near = 0.1;
	float far = 5000.0;

	float z = currentDepth * 2.0 - 1.0;
	float linearDepth = (2.0 * near * far) / (far + near - z * (far - near));

	float fogFactor = clamp((linearDepth - fogStart) / (fogEnd - fogStart), 0.0, 1.0);
	fogFactor = smoothstep(0.0, 1.0, fogFactor);

	finalColor = mix(finalColor, fogColor, fogFactor);
	FragColor = vec4(finalColor, 1.0);
}
