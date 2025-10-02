#version 430 core

out vec4 FragColor;
in vec2 texCoords;

uniform sampler2D screenTexture;
uniform sampler2D depthTexture;
uniform int isUnderwater;

uniform vec2 texelSize;

const float depthSkyThreshold = 1;

void main()
{
	vec3 currentColor = texture(screenTexture, texCoords).rgb;
	float currentDepth = texture(depthTexture, texCoords).r;
	vec3 finalColor = currentColor;
	float finalDepth = currentDepth;

	bool isSkyDepth = currentDepth >= depthSkyThreshold;

		if (isSkyDepth)
		{
			// Only fix depth discontinuities; preserve color to avoid smearing post-effects like god rays
			float upDepth		= texture(depthTexture, texCoords + vec2(0.0,  texelSize.y)).r;
			float downDepth		= texture(depthTexture, texCoords + vec2(0.0,  -texelSize.y)).r;
			float leftDepth		= texture(depthTexture, texCoords + vec2(-texelSize.x, 0.0)).r;
			float rightDepth	= texture(depthTexture, texCoords + vec2( texelSize.x, 0.0)).r;

			float bDepth	= (upDepth + downDepth + leftDepth + rightDepth) / 4;
			finalColor = currentColor; // keep existing color (which may include god rays)
			finalDepth = bDepth;
		}
	else
	{
		finalColor = currentColor;
		finalDepth = currentDepth;
	}

	gl_FragDepth = finalDepth;
	FragColor = vec4(finalColor, 1.0);
}
