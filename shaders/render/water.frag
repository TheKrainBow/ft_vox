#version 430 core

// ============================================================================
// TWEAKABLES CONSTANTS
// ============================================================================
// Waves (matches simple sin/cos surface)
const float WAVE_FREQ			= 0.001;
const float WAVE_AMP			= 0.03;
const float WAVE_SPEED			= 0.01;

// Normal-map ripples
const float RIPPLE_UV_SCALE			= 0.05;
const float RIPPLE_SCROLL_X			= 0.0002;
const float RIPPLE_SCROLL_Y			= 0.00015;
const float RIPPLE_STRENGTH			= 0.05;   // 0..1, blend toward up-vector to soften
const float NORMAL_Z_ATTENUATION	= 0.10;   // <1 flattens screen-space Z wobble

// Second ripple layer to reduce tiling/repetition
const float RIPPLE2_UV_SCALE	= 0.020;   // larger ripples
const float RIPPLE2_SCROLL_X	= -0.00012;
const float RIPPLE2_SCROLL_Y	=  0.00009;
const float RIPPLE_LERP			= 0.50;	// 0..1 blend of layer1→layer2


// Reflection + colors
const float REFLECT_DISTANCE	= 5000.0;
const vec3  SKY_COLOR			= vec3(0.53, 0.81, 0.92);
const vec3  BLUE_TINT			= vec3(0.10, 0.20, 0.50);
const vec3  WATER_BASE			= vec3(0.07, 0.14, 0.55);
const vec3  UNDERWATER_COLOR	= vec3(0.00, 0.10, 0.30);

// Distance → color gradient (near = turquoise, far = deep blue)
const vec3  NEAR_TURQUOISE	= vec3(0.07, 0.24, 0.28);
const vec3  FAR_DEEP_BLUE	= vec3(0.02, 0.07, 0.18);
const float TINT_NEAR_DIST	= 5.0;	// start of fade (meters)
const float TINT_FAR_DIST	= 50.0;	// end of fade (meters)

// Height fade (weakens reflection as camera rises above the surface)
const float HEIGHT_FADE_RANGE	= 25.0;
const float HEIGHT_FADE_MAX		= 0.80;

// Small brightness lift from normal B channel (micro-variation)
const float NORMAL_OFFSET_SCALE  = 0.20;

// Fresnel / overall reflection strength
const float FRESNEL_F0				= 0.02; // ~2% for water
const float REFLECTION_STRENGTH_CAP	= 0.65; // clamp max reflection contribution
const float REFLECTION_BLUE_MIX		= 0.10; // bias reflection slightly toward blue

// Optional tweaks
const bool  USE_FRAGMENT_Y_WAVE	= false; // add per-fragment Y wave (keep false if vertex animates)
const bool  USE_REFLECTION_BLUR	= false; // small 4-tap blur to soften mirror look
const float BLUR_UV_OFFSET		= 0.001;

// Choose reflection source:
// 0 = sample screenTexture normally
// 1 = ALWAYS use SKY_COLOR  <-- default
// 2 = screenTexture but fall back to SKY_COLOR when OOB/behind camera
const int   REFLECTION_SOURCE	= 2;

// Optional tonemapping for reflection (kept OFF to avoid gray look)
const bool  USE_REFLECTION_TONE	= false;
const float REFLECTION_GAMMA	= 1.60;   // used only if USE_REFLECTION_TONE = true
const float REFLECTION_SCALE	= 0.65;   // used only if USE_REFLECTION_TONE = true

// Final alpha
const float ALPHA_NEAR_VALUE	= 0.80;
const float ALPHA_FAR_VALUE		= 0.90;
const float ALPHA_NEAR_DIST		= 5.0;
const float ALPHA_FAR_DIST		= 50.0;

// ============================================================================
// REQUIRED UNIFORMS
// ============================================================================
in vec3 FragPos;

uniform sampler2D screenTexture;
uniform sampler2D normalMap;

uniform vec3  viewPos;
uniform float time;
uniform mat4  view;
uniform mat4  projection;
uniform int   isUnderwater;
uniform float waterHeight;

out vec4 FragColor;


// ============================================================================
// Helpers
// ============================================================================
float fresnelSchlick(float cosTheta, float F0) {
	return F0 + (1.0 - F0) * pow(max(1.0 - cosTheta, 0.0), 5.0);
}

float calcWave(vec3 pos) {
	float px = pos.x * WAVE_FREQ + time * WAVE_SPEED;
	float pz = pos.z * WAVE_FREQ + time * WAVE_SPEED * 0.8;
	float w  = sin(px) + cos(pz);
	return 0.5 * WAVE_AMP * w;
}

float computeHeightFade(vec3 cam, vec3 frag) {
	float cameraHeight = cam.y - frag.y;
	float base = 1.0 - (cameraHeight / HEIGHT_FADE_RANGE);
	return clamp(base, 0.0, HEIGHT_FADE_MAX);
}

struct NormalInfo {
	vec3  normal;
	float offset;
};

NormalInfo computeWaterNormal(vec3 fragPos) {
	vec2 uv1 = fragPos.xz * RIPPLE_UV_SCALE
				+ vec2(time * RIPPLE_SCROLL_X, time * RIPPLE_SCROLL_Y);
	vec2 uv2 = fragPos.xz * RIPPLE2_UV_SCALE
				+ vec2(time * RIPPLE2_SCROLL_X, time * RIPPLE2_SCROLL_Y);

	vec3 s1 = texture(normalMap, uv1).rgb;
	vec3 s2 = texture(normalMap, uv2).rgb;

	vec3 n1 = s1 * 2.0 - 1.0;
	vec3 n2 = s2 * 2.0 - 1.0;

	vec3 n = normalize(mix(n1, n2, RIPPLE_LERP));
	n = normalize(mix(vec3(0.0, 1.0, 0.0), n, RIPPLE_STRENGTH));
	n.z *= NORMAL_Z_ATTENUATION;

	NormalInfo info;
	info.normal = n;
	info.offset = ((mix(s1.b, s2.b, RIPPLE_LERP) - 0.5) * NORMAL_OFFSET_SCALE);
	return info;
}

vec2 toUV(vec4 clip, out bool outOfBounds) {
	vec3 ndc = clip.xyz / clip.w;
	vec2 uv  = ndc.xy * 0.5 + 0.5;
	outOfBounds = any(lessThan(uv, vec2(0.0))) || any(greaterThan(uv, vec2(1.0)));
	return uv;
}

vec3 sampleReflection(vec2 uv) {
	if (USE_REFLECTION_BLUR) {
		vec3 c = vec3(0.0);
		c += texture(screenTexture, uv + vec2( BLUR_UV_OFFSET, 0.0)).rgb;
		c += texture(screenTexture, uv + vec2(-BLUR_UV_OFFSET, 0.0)).rgb;
		c += texture(screenTexture, uv + vec2(0.0,  BLUR_UV_OFFSET)).rgb;
		c += texture(screenTexture, uv + vec2(0.0, -BLUR_UV_OFFSET)).rgb;
		return c * 0.25;
	}
	return texture(screenTexture, uv).rgb;
}

vec3 toneReflection(vec3 color) {
	if (!USE_REFLECTION_TONE) return color;
	vec3 toned = pow(max(color, vec3(0.0)), vec3(REFLECTION_GAMMA)) * REFLECTION_SCALE;
	return toned;
}

// Camera distance based methods for color and opacity
vec3 distanceTint(vec3 cam, vec3 frag) {
	float d = length(cam - frag);
	float t = smoothstep(TINT_NEAR_DIST, TINT_FAR_DIST, d); // 0 near → 1 far
	return mix(NEAR_TURQUOISE, FAR_DEEP_BLUE, t);
}

float distanceAlpha(vec3 cam, vec3 frag) {
	float d = length(cam - frag);
	float t = smoothstep(ALPHA_NEAR_DIST, ALPHA_FAR_DIST, d); // 0 near → 1 far (clamped)
	return mix(ALPHA_NEAR_VALUE, ALPHA_FAR_VALUE, t);
}

// ============================================================================
// Main
// ============================================================================
void main() {
	vec3 waveFragPos = FragPos;
	if (USE_FRAGMENT_Y_WAVE) {
		waveFragPos.y += calcWave(FragPos);
	}

	vec3 viewDir = normalize(waveFragPos - viewPos);
	NormalInfo nfo = computeWaterNormal(waveFragPos);
	float heightFade = computeHeightFade(viewPos, waveFragPos);

	// Reflection ray/project
	vec3 reflectedDir	= reflect(viewDir, nfo.normal);
	vec3 reflectedPoint	= waveFragPos + reflectedDir * REFLECT_DISTANCE;
	vec4 clip			= projection * view * vec4(reflectedPoint, 1.0);

	// Determine base reflection color per selected source
	vec3 reflectedBase;
	if (REFLECTION_SOURCE == 1) {
		// Always the nice sky color
		reflectedBase = SKY_COLOR;
	}
	else if (REFLECTION_SOURCE == 0) {
		// Always sample screen texture
		bool dummyOOB = false;
		vec2 uv = (clip.w <= 0.0) ? vec2(0.5) : toUV(clip, dummyOOB);
		reflectedBase = sampleReflection(uv);
	}
	else { // REFLECTION_SOURCE == 2
		// Sample screen; fall back to sky when OOB/behind camera
		bool outOfBounds = (clip.w <= 0.0);
		vec2 uv = outOfBounds ? vec2(0.5) : toUV(clip, outOfBounds);
		reflectedBase = outOfBounds ? SKY_COLOR : sampleReflection(uv);
	}

	// Gentle blue bias and optional tone (tone OFF by default to avoid gray)
	vec3 reflection = mix(reflectedBase, BLUE_TINT, REFLECTION_BLUE_MIX);
	reflection = toneReflection(reflection);

	// Fresnel with low F0 (limits straight-on reflectance)
	float cosTheta	= max(dot(-viewDir, nfo.normal), 0.0);
	float fres		= fresnelSchlick(cosTheta, FRESNEL_F0);

	float baseStrength	= clamp(fres * heightFade, 0.0, 1.0);
	float reflMix		= baseStrength * REFLECTION_STRENGTH_CAP;

	vec3 finalColor;
	if (isUnderwater == 1)
	{
		// Underwater: no reflections; use same distance-based tinting as outside
		vec3 baseTintUW = distanceTint(viewPos, waveFragPos);
		finalColor = baseTintUW;
		finalColor += nfo.offset;
		finalColor = clamp(finalColor, 0.0, 1.0);
	}
	else
	{
		finalColor  = mix(WATER_BASE, reflection, reflMix);
		finalColor += nfo.offset;
		finalColor  = clamp(finalColor, 0.0, 1.0);
	}

    float alpha = distanceAlpha(viewPos, waveFragPos);
    if (isUnderwater == 1) {
		// Underwater: reduce opacity so above-surface scene remains visible
		float depthUnder = clamp((waterHeight - viewPos.y) / 30.0, 0.0, 1.0);
		float cosTheta = max(dot(-viewDir, nfo.normal), 0.0);
		float viewFactor = 1.0 - cosTheta;
		float alphaUW = mix(0.25, 0.60, depthUnder);
		alphaUW *= mix(0.60, 1.00, viewFactor);
		alphaUW = clamp(alphaUW, 0.15, 0.70);
		FragColor = vec4(finalColor, alphaUW);
		return;
    }

	vec3 baseTint = distanceTint(viewPos, waveFragPos);
	finalColor    = mix(baseTint, reflection, reflMix);
	FragColor     = vec4(finalColor, alpha);
}
