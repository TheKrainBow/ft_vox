#version 430 core

// ============================================================================
// TWEAKABLES CONSTANTS
// ============================================================================
// Waves (matches simple sin/cos surface)
const float WAVE_FREQ            = 0.001;
const float WAVE_AMP             = 0.03;
const float WAVE_SPEED           = 0.01;

// Normal-map ripples
const float RIPPLE_UV_SCALE         = 0.05;
const float RIPPLE_SCROLL_X         = 0.0002;
const float RIPPLE_SCROLL_Y         = 0.00015;
const float RIPPLE_STRENGTH         = 0.05;   // 0..1, blend toward up-vector to soften
const float NORMAL_Z_ATTENUATION    = 0.10;   // <1 flattens screen-space Z wobble

// Second ripple layer to reduce tiling/repetition
const float RIPPLE2_UV_SCALE    = 0.020;   // larger ripples
const float RIPPLE2_SCROLL_X    = -0.00012;
const float RIPPLE2_SCROLL_Y    =  0.00009;
const float RIPPLE_LERP         = 0.50;   // 0..1 blend of layer1→layer2

// Reflection + colors
const float REFLECT_DISTANCE    = 5000.0;
const vec3  SKY_COLOR           = vec3(0.53, 0.81, 0.92);
const vec3  BLUE_TINT           = vec3(0.10, 0.20, 0.50);
const vec3  WATER_BASE          = vec3(0.07, 0.14, 0.55);
const vec3  UNDERWATER_COLOR    = vec3(0.00, 0.10, 0.30);

// Distance → color gradient (near = turquoise, far = deep blue)
const vec3  NEAR_TURQUOISE      = vec3(0.07, 0.24, 0.28);
const vec3  FAR_DEEP_BLUE       = vec3(0.02, 0.07, 0.18);
// Debug wireframe color: bright, saturated blue for high visibility
const vec3  DEBUG_WIREFRAME_BLUE = vec3(0.12, 0.55, 1.00);
const float TINT_NEAR_DIST      = 5.0;   // start of fade (meters)
const float TINT_FAR_DIST       = 50.0;  // end of fade (meters)

// Height fade (weakens reflection as camera rises above the surface)
const float HEIGHT_FADE_RANGE   = 25.0;
const float HEIGHT_FADE_MAX     = 0.80;

// Small brightness lift from normal B channel (micro-variation)
const float NORMAL_OFFSET_SCALE = 0.20;

// Fresnel / overall reflection strength
const float FRESNEL_F0              = 0.02; // ~2% for water
const float REFLECTION_STRENGTH_CAP = 0.65; // clamp max reflection contribution
const float REFLECTION_BLUE_MIX     = 0.10; // bias reflection slightly toward blue

// Optional tweaks
const bool  USE_FRAGMENT_Y_WAVE = false; // add per-fragment Y wave (keep false if vertex animates)
const bool  USE_REFLECTION_BLUR = false; // small 4-tap blur to soften mirror look
const float BLUR_UV_OFFSET      = 0.001;

// Optional tonemapping for reflection (kept OFF to avoid gray look)
const bool  USE_REFLECTION_TONE = false;
const float REFLECTION_GAMMA    = 1.60;   // used only if USE_REFLECTION_TONE = true
const float REFLECTION_SCALE    = 0.65;   // used only if USE_REFLECTION_TONE = true

// Final alpha
const float ALPHA_NEAR_VALUE    = 0.80;
const float ALPHA_FAR_VALUE     = 0.90;
const float ALPHA_NEAR_DIST     = 5.0;
const float ALPHA_FAR_DIST      = 50.0;

// ============================================================================
// REQUIRED UNIFORMS
// ============================================================================
in vec3 FragPos;
in vec3 Normal; // face normal from geometry (world space)
in vec2 TexCoord;
flat in int TextureID;

uniform sampler2D screenTexture;
// Opaque pass depth buffer (0..1). Used to cull obviously-wrong SSR hits
// such as foreground objects between the camera and the water.
uniform sampler2D depthTexture;
uniform sampler2DArray textureArray;
uniform sampler2D normalMap;

uniform vec3  viewPos;
uniform vec3  lightColor;
uniform float time;
uniform mat4  view;         // rotation-only (unchanged)
uniform mat4  viewOpaque;
uniform mat4  projection;
uniform int   isUnderwater;
uniform float waterHeight;
// Needed to linearize sampled scene depth
uniform float nearPlane;
uniform float farPlane;
uniform bool  showtrianglemesh; // debugging: true -> force deep blue, no reflections

// SSR offscreen fallback
uniform sampler2D planarTexture;
uniform mat4  planarView;
uniform mat4  planarProjection;

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

struct NormalInfo { vec3 normal; float offset; };

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

// Convert depth in [0,1] (non-linear) to view-space distance from camera (meters)
float linearizeDepth(float z01)
{
    // OpenGL: z_ndc in [-1,1] maps to depth in [0,1]
    float z = z01 * 2.0 - 1.0;
    return (2.0 * nearPlane * farPlane) / (farPlane + nearPlane - z * (farPlane - nearPlane));
}

vec3 toneReflection(vec3 color) {
	if (!USE_REFLECTION_TONE) return color;
	vec3 toned = pow(max(color, vec3(0.0)), vec3(REFLECTION_GAMMA)) * REFLECTION_SCALE;
	return toned;
}

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

vec3 samplePlanar(vec3 refPointWS, out bool outOfBounds) {
	vec4 c = planarProjection * planarView * vec4(refPointWS, 1.0);
	if (c.w <= 0.0) { outOfBounds = true; return SKY_COLOR; }
	vec2 uv = toUV(c, outOfBounds);
	return outOfBounds ? SKY_COLOR : texture(planarTexture, uv).rgb;
}

// tiny hash for dithering the seam (breaks the straight line)
float hash12(vec2 p) {
	return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

// Planar sample that also returns UV so we can blur only near seam
vec3 samplePlanarUV(vec3 refPointWS, out vec2 uv, out bool outOfBounds) {
	vec4 c = planarProjection * planarView * vec4(refPointWS, 1.0);
	if (c.w <= 0.0) { outOfBounds = true; uv = vec2(0.5); return SKY_COLOR; }
	uv = toUV(c, outOfBounds);
	return outOfBounds ? SKY_COLOR : texture(planarTexture, uv).rgb;
}

vec3 samplePlanarBlur(vec2 uv) {               // very small 5-tap blur
	vec2 texel = 1.0 / vec2(textureSize(planarTexture, 0));
	vec3 c  = texture(planarTexture, uv).rgb;
			c += texture(planarTexture, uv + vec2( texel.x, 0.0)).rgb;
			c += texture(planarTexture, uv + vec2(-texel.x, 0.0)).rgb;
			c += texture(planarTexture, uv + vec2(0.0,  texel.y)).rgb;
			c += texture(planarTexture, uv + vec2(0.0, -texel.y)).rgb;
	return c * 0.2;
}

// ============================================================================
// Main
// ============================================================================
// Sun helper reused for simple leaf lighting (time used as day time)
vec3 computeSunPosition(float t, vec3 camPos) {
    const float pi = 3.14159265359;
    const float radius = 6000.0;
    const float dayStart = 42000.0;
    const float dayLen   = 86400.0 - dayStart;
    const float nightLen = dayStart;
    float angle;
    if (t < dayStart) { float phase = clamp(t / nightLen, 0.0, 1.0); angle = pi + phase * pi; }
    else { float phase = clamp((t - dayStart) / dayLen, 0.0, 1.0); angle = phase * pi; }
    return camPos + vec3(radius * cos(angle), radius * sin(angle), 0.0);
}

void main() {
    // Non-water transparent surfaces (e.g., leaves) rendered with terrain atlas
    if (TextureID != 6) {
        vec4 tex = texture(textureArray, vec3(TexCoord, TextureID));
        // Masked alpha: discard fully/mostly transparent texels so depth works
        if (tex.a < 0.5) discard;
        vec3 sunPos = computeSunPosition(time, viewPos);
        vec3 L = normalize(sunPos - FragPos);
        vec3 N = normalize(Normal);
        // Terrain-like lighting mix
        float diffuse = max(dot(N, L), 0.0);
        // Day progression for diffuse scale (match terrain.frag conceptually)
        const float pi = 3.14159265359;
        const float dayStart = 42000.0;
        const float dayLen   = 86400.0 - dayStart;
        float dayPhase = clamp((time - dayStart) / dayLen, 0.0, 1.0);
        float sun = sin(dayPhase * pi);
        sun = smoothstep(0.0, 0.15, sun);
        float ambient = mix(0.10, 0.35, sun); // darker nights, brighter days
        float finalDiffuse = diffuse * 0.9 * (0.2 * sun);
        float totalLight = clamp(ambient * 0.6 + finalDiffuse + 0.1, 0.0, 1.0);
        vec3 color = tex.rgb * lightColor * totalLight;
        FragColor = vec4(color, 1.0);
        return;
    }
	// Debug/wireframe mode: bright, saturated blue for clear visibility
	if (showtrianglemesh) {
		FragColor = vec4(DEBUG_WIREFRAME_BLUE, 1.0);
		return;
	}
	vec3 waveFragPos = FragPos;
	if (USE_FRAGMENT_Y_WAVE) {
		waveFragPos.y += calcWave(FragPos);
	}

	vec3 viewDir = normalize(waveFragPos - viewPos);
	NormalInfo nfo = computeWaterNormal(waveFragPos);
	float heightFade = computeHeightFade(viewPos, waveFragPos);

	// Water fragment screen-space depth (used for a coarse SSR occlusion test)
	vec4 waterClip = projection * viewOpaque * vec4(waveFragPos, 1.0);
	float waterDepth01 = clamp(waterClip.z / waterClip.w * 0.5 + 0.5, 0.0, 1.0);
	float waterDepthLin = linearizeDepth(waterDepth01);

	// Reflection ray/project
	vec3 reflectedDir   = reflect(viewDir, nfo.normal);
	vec3 reflectedPoint = waveFragPos + reflectedDir * REFLECT_DISTANCE;
	vec4 clip           = projection * viewOpaque * vec4(reflectedPoint, 1.0);

	// Determine base reflection color
	vec3 reflectedBase;
	bool oobPlanar = false; vec2 uvPlanar; vec3 planarCol = samplePlanarUV(reflectedPoint, uvPlanar, oobPlanar);
	reflectedBase = oobPlanar ? SKY_COLOR : planarCol;
	

	vec3 reflection = mix(reflectedBase, BLUE_TINT, REFLECTION_BLUE_MIX);
	reflection = toneReflection(reflection);

	float cosTheta = max(dot(-viewDir, nfo.normal), 0.0);
	float fres     = fresnelSchlick(cosTheta, FRESNEL_F0);

	float baseStrength = clamp(fres * heightFade, 0.0, 1.0);
	float reflMix      = baseStrength * REFLECTION_STRENGTH_CAP;

	// Disable reflections on downward and side faces.
	// Keep reflections only for upward-facing surfaces (Ny sufficiently positive).
	float ny = normalize(Normal).y;
	if (ny < 0.5) {
		reflMix = 0.0;
	}

	vec3 finalColor;
	if (isUnderwater == 1)
	{
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
		float depthUnder = clamp((waterHeight - viewPos.y) / 30.0, 0.0, 1.0);
		float cosThetaUW = max(dot(-viewDir, nfo.normal), 0.0);
		float viewFactor = 1.0 - cosThetaUW;
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
