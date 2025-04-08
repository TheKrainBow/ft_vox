#version 430 core

out vec4 FragColor;
in vec2 texCoords;

uniform sampler2D screenTexture;
uniform sampler2D depthTexture;
uniform int timeValue;

uniform vec2 texelSize;

// how far up to look for confirmation
const float skyOffsetY = 5.0;

// tweak this based on how far your skybox is
const float depthSkyThreshold = 1;

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

void main()
{
	vec3 currentColor = texture(screenTexture, texCoords).rgb;
	float currentDepth = texture(depthTexture, texCoords).r;

	bool isSkyDepth = currentDepth >= depthSkyThreshold;

	if (isSkyDepth)
	{
		// Look upwards to make sure it's part of the skybox, not a crack
		float skyCheckUpDepth = texture(depthTexture, texCoords + vec2(0.0, skyOffsetY * texelSize.y)).r;
		float skyCheckDownDepth = texture(depthTexture, texCoords - vec2(0.0, skyOffsetY * texelSize.y)).r;
		bool isTrueSky = skyCheckUpDepth >= depthSkyThreshold || skyCheckDownDepth >= depthSkyThreshold;
		// vec3 currentColor = computeSkyColor(timeValue);

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
