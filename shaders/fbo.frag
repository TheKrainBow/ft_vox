#version 430 core

out vec4 FragColor;
in vec2 texCoords;

uniform sampler2D screenTexture;
uniform sampler2D depthTexture;
uniform int timeValue;

uniform vec2 texelSize;

// Offset to check for sky
const float skyOffsetY = 5.0;

// Skybox far
const float depthSkyThreshold = 0.9999;

// Fog constants
const float fogStart = 40.0;
const float fogEnd = 500.0;

vec3 computeSkyColor(float time)
{
    float t = mod(time, 86400.0); // Ensure wraparound

    vec3 nightColor   = vec3(0.05, 0.07, 0.2);
    vec3 dawnColor    = vec3(0.8, 0.5, 0.3);
    vec3 dayColor     = vec3(0.53, 0.81, 0.92);
    vec3 duskColor    = vec3(0.8, 0.4, 0.2);

    vec3 skyColor;

    if (t < 21600.0) {
        // Night → Dawn (0–6h)
        float f = t / 21600.0;
        skyColor = mix(nightColor, dawnColor, f);
    } else if (t < 43200.0) {
        // Dawn → Day (6–12h)
        float f = (t - 21600.0) / 21600.0;
        skyColor = mix(dawnColor, dayColor, f);
    } else if (t < 64800.0) {
        // Day → Dusk (12–18h)
        float f = (t - 43200.0) / 21600.0;
        skyColor = mix(dayColor, duskColor, f);
    } else {
        // Dusk → Night (18–24h)
        float f = (t - 64800.0) / 21600.0;
        skyColor = mix(duskColor, nightColor, f);
    }

    return skyColor;
}

float calculateFogFactor(float depth)
{
	float linearDepth = (2.0 * 0.1 * 1000.0) / (1000.0 + 0.1 - depth * (1000.0 - 0.1));

	float fogFactor = clamp((linearDepth - fogStart) / (fogEnd - fogStart), 0.0, 1.0);
	return fogFactor;
}

void main()
{
	vec3 currentColor = texture(screenTexture, texCoords).rgb;
	float currentDepth = texture(depthTexture, texCoords).r;
	bool isSkyDepth = currentDepth >= depthSkyThreshold;
	vec3 fogColor = computeSkyColor(timeValue);
	float fogFactor;

	if (isSkyDepth)
	{
		float skyCheckUpDepth = texture(depthTexture, texCoords + vec2(0.0, skyOffsetY * texelSize.y)).r;
		bool isTrueSky = skyCheckUpDepth >= depthSkyThreshold;

		if (isTrueSky)
		{
			FragColor = vec4(currentColor, 1.0);
			return ;
		}
		else
		{
			vec3 up    = texture(screenTexture, texCoords + vec2(0.0,  texelSize.y)).rgb;
			vec3 down  = texture(screenTexture, texCoords + vec2(0.0, -texelSize.y)).rgb;
			vec3 left  = texture(screenTexture, texCoords + vec2(-texelSize.x, 0.0)).rgb;
			vec3 right = texture(screenTexture, texCoords + vec2( texelSize.x, 0.0)).rgb;
			vec3 blended = (up + down + left + right) / 4.0;

			float neighborDepth = texture(depthTexture, texCoords + vec2(texelSize.x, 0.0)).r;
			fogFactor = calculateFogFactor(neighborDepth);
			currentColor = mix(blended, fogColor, fogFactor);
			FragColor = vec4(currentColor, 1.0);
		}
	}
	else
	{
		fogFactor = calculateFogFactor(currentDepth);
		currentColor = mix(currentColor, fogColor, fogFactor);
		FragColor = vec4(currentColor, 1.0);
	}
}
