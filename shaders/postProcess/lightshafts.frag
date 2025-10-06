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
const int   numSamplesDry = 56;
const int   numSamplesWet = 84; // more steps when underwater for denser rays

// Underwater-specific boosts (kept modest; fog will still tame brightness)
const float UW_DENSITY_BOOST  = 1.35;
const float UW_WEIGHT_BOOST   = 1.45;
const float UW_EXPOSURE_BOOST = 1.08;

float skyMask(vec2 uv)
{
	// Classify true sky using a strict threshold so underwater fogged regions don't count as sky.
	float d = texture(depthTexture, uv).r;
	float thr = (isUnderwater == 1) ? 0.9996 : 0.997; // much stricter underwater
	return smoothstep(thr, 1.0, d);
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

	// Effective params (slightly stronger underwater for porous occluders)
	float densityEff  = density  * (isUnderwater == 1 ? UW_DENSITY_BOOST  : 1.0);
	float weightEff   = weight   * (isUnderwater == 1 ? UW_WEIGHT_BOOST   : 1.0);
	float exposureEff = exposure * (isUnderwater == 1 ? UW_EXPOSURE_BOOST : 1.0);

	// Accumulate samples from fragment toward the sun
	// Clamp sun position to viewport to avoid popping at edges
	vec2 sunClamped = clamp(sunScreen, vec2(0.0), vec2(1.0));
	int  samples    = (isUnderwater == 1) ? numSamplesWet : numSamplesDry;
	vec2 delta = (texCoords - sunClamped) * (densityEff / float(samples));
	vec2 coord = texCoords;
	float illum = 1.0;
	float occlusion = 0.0;

	for (int i = 0; i < samples; ++i) {
		coord -= delta;
		// Stop when outside the screen
		if (coord.x < 0.0 || coord.x > 1.0 || coord.y < 0.0 || coord.y > 1.0)
			break;
		float sky = skyMask(coord);
		occlusion += sky * illum * weightEff;
		illum *= decay;
	}

	// Tint: warm above water, cooler underwater
	vec3 tintAbove = vec3(1.0, 0.96, 0.85);
	vec3 tintUnder = vec3(0.5, 0.7, 1.0);
	vec3 tint = (isUnderwater == 1) ? tintUnder : tintAbove;

	// Slightly reduce glare when looking straight at the sun (above water only)
	float r = length(texCoords - sunClamped);
	float nearSun = 1.0 - smoothstep(0.00, 0.18, r);
	float glareAtten = 1.0 - ((isUnderwater == 1) ? 0.30 : 0.45) * nearSun;
	vec3 rays = tint * occlusion * (exposureEff * rayIntensity * edgeFade * glareAtten);
	vec3 color = base + rays;

	FragColor = vec4(color, 1.0);
}
