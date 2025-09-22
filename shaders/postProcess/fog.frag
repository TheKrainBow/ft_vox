#version 430 core

out vec4 FragColor;
in vec2 texCoords;

uniform sampler2D screenTexture;
uniform sampler2D depthTexture;
uniform float renderDistance;
uniform int isUnderwater;
uniform float nearPlane;
uniform float farPlane;
uniform vec3 viewPos;
uniform float waterHeight;

// Fog controls
const float UW_MIN_TINT_DEPTH = 1.5;   // meters below surface where tint begins
const float UW_MAX_TINT_DEPTH = 28.0;  // meters where tint is fully applied
const float UW_DENSITY_BASE   = 0.008; // base density at shallow depth
const float UW_DENSITY_DEEP   = 0.035; // density at deep depth
const float UW_DENSITY_EXP    = 1.6;   // >1 slows the density ramp progression
const float UW_DISTANCE_SPEED = 1.5;   // >1 makes distance fog accumulate faster underwater
const float SKY_DEPTH_START = 0.999;   // classify as sky when depth >= this
const float SKY_FOG_CAP     = 0.12;    // max fog amount allowed on sky (0 to disable)
uniform float skyDepthThreshold;
uniform vec3 fogColor;


// Fog helpers
float linearizeDepth(float depth)
{
	// Convert non-linear depth buffer value to linear eye-space depth
	float z = depth * 2.0 - 1.0; // NDC
	return (2.0 * nearPlane * farPlane) / (farPlane + nearPlane - z * (farPlane - nearPlane));
}

void main() {
	vec3 sceneSRGB = texture(screenTexture, texCoords).rgb;
	float currentDepth = texture(depthTexture, texCoords).r;
	vec3 finalColor = sceneSRGB;

	// gamma -> linear
	vec3 sceneLin = pow(sceneSRGB, vec3(2.2));
	vec3 fogLin   = pow(fogColor,  vec3(2.2));

	// Use uniforms for linearization
	float linearDepth = linearizeDepth(currentDepth);

	if (isUnderwater == 1) {
		float depthMeters = max(0.0, waterHeight - viewPos.y);
		float depthUnder  = smoothstep(UW_MIN_TINT_DEPTH, UW_MAX_TINT_DEPTH, depthMeters);
		vec3 shallowAqua = vec3(0.07, 0.24, 0.28);
		vec3 deepBlue    = vec3(0.00, 0.12, 0.25);
		vec3 uwTint      = mix(shallowAqua, deepBlue, depthUnder);

		float linDepthUW = linearDepth;

		float depthRamp = pow(depthUnder, UW_DENSITY_EXP);
		float maxPath = mix(80.0, 320.0, depthRamp);
		float effDepth = min(linDepthUW * UW_DISTANCE_SPEED, maxPath);

		float density = mix(UW_DENSITY_BASE * 2.0, UW_DENSITY_DEEP, depthRamp);
		float fog = 1.0 - exp(-density * effDepth);

		const float skyDepthStart = 0.995;
		const float skyDepthEnd   = 0.9999;
		float skyRamp  = smoothstep(skyDepthStart, skyDepthEnd, currentDepth);
		float skyAtten = mix(1.0, 0.9, skyRamp);
		fog *= skyAtten;

		float gray = dot(finalColor, vec3(0.299, 0.587, 0.114));
		finalColor = mix(finalColor, vec3(gray), 0.12);
		finalColor = mix(finalColor, uwTint, fog);
	}
	else
	{
		float fogNear   = renderDistance * 6.0;
		float fogRange  = renderDistance * 20.0;

		// Distance beyond the near threshold
		float d = max(0.0, linearDepth - fogNear);

		// Mild extinction (Beer–Lambert); smaller = softer
		const float OUT_DENSITY = 0.0014;
		float fog = 1.0 - exp(-OUT_DENSITY * d);

		// Ease so it doesn’t become dense at fogNear
		float t = clamp(d / max(fogRange, 1e-3), 0.0, 1.0);
		t = t * t * (3.0 - 2.0 * t);
		fog *= t;

		// Never let it get fully opaque
		const float OUT_MAX = 0.78;
		fog = min(fog, OUT_MAX);

		// Blend
		vec3 mixedLin  = mix(sceneLin, fogLin, fog);
		vec3 mixedSRGB = pow(mixedLin, vec3(1.0/2.2));
		finalColor = mixedSRGB;
	}
	FragColor = vec4(finalColor, 1.0);
}
