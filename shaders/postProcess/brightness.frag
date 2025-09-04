#version 430 core

uniform sampler2D depthTexture;
uniform float near;
uniform float far;

in vec2 texCoords;
out vec4 FragColor;

float linearizeDepth(float z)
{
	float zN = z * 2.0 - 1.0; // NDC to clip space
	return (2.0 * near * far) / (far + near - zN * (far - near));
}

void main()
{
	float depthSample = texture(depthTexture, texCoords).r;

	// If depth is 1.0 it's sky
	if (depthSample >= 1.0) {
		FragColor = vec4(0.0);
		return;
	}

	float depth = linearizeDepth(depthSample);
	float normalizedDepth = 1.0 - clamp((depth - near) / (far - near), 0.0, 1.0);

	FragColor = vec4(vec3(normalizedDepth), 1.0); // grayscale output
}
