#version 430 core

// Single-pass light shafts (godrays)
// - Reads scene color and depth
// - Projects sun to screen space
// - Accumulates sky visibility along ray toward the sun
// - Blends colored shafts onto the scene

uniform sampler2D screenTexture;   // scene color
uniform sampler2D depthTexture;    // scene depth (non-linear)

uniform vec3 sunPos;               // world-space sun position
uniform mat4 view;
uniform mat4 projection;

uniform float rayIntensity;        // 0..1 from CPU (sun height, etc.)
uniform int   isUnderwater;        // 1 if camera underwater

in vec2 texCoords;
out vec4 FragColor;

// Tunables
// Slightly boosted to bring shafts out through porous occluders (trees)
const float exposure   = 1.05;  // was 0.90
const float decay      = 0.94;
const float density    = 0.40;  // was 0.35
const float weight     = 0.026; // was 0.020
const int   numSamples = 56;

float skyMask(vec2 uv)
{
    // Treat very-far depth as sky, but use a smooth threshold so thin foliage doesn’t entirely cancel rays.
    float d = texture(depthTexture, uv).r;
    return smoothstep(0.997, 1.0, d);
}

void main()
{
	vec3 base = texture(screenTexture, texCoords).rgb;

    // Sun to NDC → screen
    vec4 viewPos = view * vec4(sunPos, 1.0);
    vec4 clip    = projection * viewPos;
    vec2 sunScreen = (clip.xy / max(1e-6, abs(clip.w))) * 0.5 + 0.5;

	// Compute how far the sun is outside the viewport (in UV units)
	float outX = max(0.0, max(-sunScreen.x, sunScreen.x - 1.0));
	float outY = max(0.0, max(-sunScreen.y, sunScreen.y - 1.0));
	float outside = length(vec2(outX, outY));

    // Smoothly fade rays as the sun leaves the screen.
    // Determine if the sun is in front of the camera using view-space Z (OpenGL forward is -Z).
    float visible = (viewPos.z < 0.0) ? 1.0 : 0.0;
	float edgeFade = 1.0 - smoothstep(0.0, 0.25, outside); // full inside → 0 over 25% margin

	// If overall intensity is zero, skip
	if (rayIntensity <= 0.0001 || visible <= 0.0) {
		FragColor = vec4(base, 1.0);
		return;
	}

	// Accumulate samples from fragment toward the sun
	// Clamp sun position to viewport to avoid popping at edges
	vec2 sunClamped = clamp(sunScreen, vec2(0.0), vec2(1.0));
	vec2 delta = (texCoords - sunClamped) * (density / float(numSamples));
	vec2 coord = texCoords;
	float illum = 1.0;
	float occlusion = 0.0;

    for (int i = 0; i < numSamples; ++i) {
        coord -= delta;
        // stop if we left the screen
        if (coord.x < 0.0 || coord.x > 1.0 || coord.y < 0.0 || coord.y > 1.0)
            break;
        float sky = skyMask(coord);
        occlusion += sky * illum * weight;
        illum *= decay;
    }

	// Tint: warm above water, cooler underwater
	vec3 tintAbove = vec3(1.0, 0.96, 0.85);
	vec3 tintUnder = vec3(0.5, 0.7, 1.0);
	vec3 tint = (isUnderwater == 1) ? tintUnder : tintAbove;

	vec3 rays = tint * occlusion * (exposure * rayIntensity * edgeFade);
	vec3 color = base + rays;

	FragColor = vec4(color, 1.0);
}
