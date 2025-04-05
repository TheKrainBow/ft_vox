#version 430 core

out vec4 FragColor;
in vec2 texCoords;

uniform sampler2D screenTexture;
uniform sampler2D depthTexture;

uniform vec2 texelSize;

// how far up to look for confirmation
const float skyOffsetY = 5.0;

// tweak this based on how far your skybox is
const float depthSkyThreshold = 0.9999;

void main()
{
	vec3 currentColor = texture(screenTexture, texCoords).rgb;
	float currentDepth = texture(depthTexture, texCoords).r;

	bool isSkyDepth = currentDepth >= depthSkyThreshold;

	if (isSkyDepth)
	{
		// Look upwards to make sure it's part of the skybox, not a crack
		float skyCheckUpDepth = texture(depthTexture, texCoords + vec2(0.0, skyOffsetY * texelSize.y)).r;
		bool isTrueSky = skyCheckUpDepth >= depthSkyThreshold;

		if (isTrueSky)
		{
			FragColor = vec4(currentColor, 1.0);
		}
		else
		{
			// Blend 4 neighbors
			vec3 up    = texture(screenTexture, texCoords + vec2(0.0,  texelSize.y)).rgb;
			vec3 down  = texture(screenTexture, texCoords + vec2(0.0, -texelSize.y)).rgb;
			vec3 left  = texture(screenTexture, texCoords + vec2(-texelSize.x, 0.0)).rgb;
			vec3 right = texture(screenTexture, texCoords + vec2( texelSize.x, 0.0)).rgb;

			vec3 blended = (up + down + left + right) / 4.0;
			FragColor = vec4(blended, 1.0);
		}
	}
	else
	{
		FragColor = vec4(currentColor, 1.0);
	}
}
