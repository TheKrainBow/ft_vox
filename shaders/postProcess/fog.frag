#version 430 core

out vec4 FragColor;
in vec2 texCoords;

uniform sampler2D screenTexture;
uniform sampler2D depthTexture;

void main()
{
	vec3 currentColor = texture(screenTexture, texCoords).rgb;
	float currentDepth = texture(depthTexture, texCoords).r;
	vec3 finalColor = currentColor;
	float finalDepth = currentDepth;

    vec3 fogColor = vec3(0.53f, 0.81f, 0.92f); // your fog color
    float fogStart = 1000.0;
    float fogEnd   = 3000.0;
	float near = 0.1f;
	float far = 5000.0f;

	float z = finalDepth * 2.0 - 1.0; // NDC to clip space
	float linearDepth = (2.0 * near * far) / (far + near - z * (far - near));

	float fogFactor = clamp((linearDepth - fogStart) / (fogEnd - fogStart), 0.0, 1.0);
	fogFactor = smoothstep(0.0, 1.0, fogFactor);

	finalColor = mix(finalColor, fogColor, fogFactor);
	FragColor = vec4(finalColor, 1.0);
	// FragColor = finalColor;
}
