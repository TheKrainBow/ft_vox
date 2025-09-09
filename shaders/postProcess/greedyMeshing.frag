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
		vec3 upColor		= texture(screenTexture, texCoords + vec2(0.0,  texelSize.y)).rgb;
		float upDepth		= texture(depthTexture, texCoords + vec2(0.0,  texelSize.y)).r;

		vec3 downColor		= texture(screenTexture, texCoords + vec2(0.0, -texelSize.y)).rgb;
		float downDepth		= texture(depthTexture, texCoords + vec2(0.0,  -texelSize.y)).r;

		vec3 leftColor		= texture(screenTexture, texCoords + vec2(-texelSize.x, 0.0)).rgb;
		float leftDepth		= texture(depthTexture, texCoords + vec2(-texelSize.x, 0.0)).r;

		vec3 rightColor		= texture(screenTexture, texCoords + vec2( texelSize.x, 0.0)).rgb;
		float rightDepth	= texture(depthTexture, texCoords + vec2( texelSize.x, 0.0)).r;

		vec3 bColor		= (upColor + downColor + rightColor + leftColor) / 4;
		float bDepth	= (upDepth + downDepth + leftDepth + rightDepth) / 4;
		finalColor = bColor;
		finalDepth = bDepth;
	}
	else
	{
		finalColor = currentColor;
		finalDepth = currentDepth;
	}

	// if (isUnderwater == 1)
	// {
	// }

	FragColor = vec4(finalColor, 1.0);
	gl_FragDepth = finalDepth;
}
