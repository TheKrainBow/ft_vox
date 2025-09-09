#version 430 core

out vec4 FragColor;
in vec2 texCoords;

uniform sampler2D screenTexture;
uniform sampler2D depthTexture;
uniform float renderDistance;
uniform int isUnderwater;
uniform float nearPlane;
uniform float farPlane;
uniform vec3 viewPos;
uniform float waterHeight;

float linearizeDepth(float depth)
{
    // Convert non-linear depth buffer value to linear eye-space depth
    float z = depth * 2.0 - 1.0; // NDC
    return (2.0 * nearPlane * farPlane) / (farPlane + nearPlane - z * (farPlane - nearPlane));
}

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

	if (isUnderwater == 1)
	{
		// FragColor = vec4(currentColor, 1.0);
        // Depth-aware underwater fog with deep blue tint
        float depthUnder = clamp((waterHeight - viewPos.y) / 30.0, 0.0, 1.0);
        vec3 deepBlue = vec3(0.0, 0.12, 0.25);

        float linDepth = linearizeDepth(currentDepth);
        float density = mix(0.025, 0.09, depthUnder); // deeper camera → denser fog
        float fog = 1.0 - exp(-density * linDepth);
        fog = clamp(fog, 0.0, 0.95);

        // slight desaturation to simulate underwater scattering
        float gray = dot(finalColor, vec3(0.299, 0.587, 0.114));
        finalColor = mix(finalColor, vec3(gray), 0.15);

        // apply color shift with fog amount
        finalColor = mix(finalColor, deepBlue, fog);
	}
	else
		finalColor = mix(finalColor, fogColor, fogFactor);
	FragColor = vec4(finalColor, 1.0);
}
