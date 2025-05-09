#version 430 core

out vec4 FragColor;
in vec2 texCoords;

uniform sampler2D screenTexture;
uniform sampler2D depthTexture;
uniform int timeValue;

uniform vec2 texelSize;

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
		vec3 up         = texture(screenTexture, texCoords + vec2(0.0,  texelSize.y)).rgb;
		float upDepth    = texture(depthTexture, texCoords + vec2(0.0,  texelSize.y)).r;

		vec3 down       = texture(screenTexture, texCoords + vec2(0.0, -texelSize.y)).rgb;
		float downDepth  = texture(depthTexture, texCoords + vec2(0.0,  -texelSize.y)).r;

		vec3 left       = texture(screenTexture, texCoords + vec2(-texelSize.x, 0.0)).rgb;
		float leftDepth  = texture(depthTexture, texCoords + vec2(-texelSize.x, 0.0)).r;

		vec3 right      = texture(screenTexture, texCoords + vec2( texelSize.x, 0.0)).rgb;
		float rightDepth = texture(depthTexture, texCoords + vec2( texelSize.x, 0.0)).r;

		vec3 blended = vec3(0.0);
		int n = 0;

		if (upDepth != 1) {
			blended += up;
			n += 1;
		}
		if (downDepth != 1) {
			blended += down;
			n += 1;
		}
		if (rightDepth != 1) {
			blended += right;
			n += 1;
		}
		if (leftDepth != 1) {
			blended += left;
			n += 1;
		}

		if (n > 0) {
			blended /= n;
			FragColor = vec4(blended, 1.0);
		} else {
			FragColor = vec4(currentColor, 1.0);
		}
	}
	else
	{
		FragColor = vec4(currentColor, 1.0);
	}
}
