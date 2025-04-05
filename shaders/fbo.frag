#version 430 core

out vec4 FragColor;
in vec2 texCoords;

uniform sampler2D screenTexture;

uniform vec2 texelSize;

const float skyOffsetY = 5.0;

const vec3 skyColor = vec3(0.53, 0.81, 0.92);
const float threshold = 0.1;

void main()
{
	vec3 currentColor = texture(screenTexture, texCoords).rgb;
	float diff = distance(currentColor, skyColor);
	bool isSkyColor = diff < threshold;

	if (isSkyColor)
	{
		vec3 skyCheckUp = texture(screenTexture, texCoords + vec2(0.0, skyOffsetY * texelSize.y)).rgb;
		bool isSky = distance(skyCheckUp, skyColor) < threshold;

		if (isSky)
			FragColor = vec4(currentColor, 1.0);
		else
		{
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
