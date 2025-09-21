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

// Underwater fog controls
// Tune these to control when tinting ramps in/out based on depth below the surface
const float UW_MIN_TINT_DEPTH = 1.5;   // meters below surface where tint begins
const float UW_MAX_TINT_DEPTH = 28.0;  // meters where tint is fully applied
const float UW_DENSITY_BASE   = 0.008; // base density at shallow depth
const float UW_DENSITY_DEEP   = 0.035; // density at deep depth
const float UW_DENSITY_EXP    = 1.6;   // >1 slows the density ramp progression
const float UW_DISTANCE_SPEED = 1.5;   // >1 makes distance fog accumulate faster underwater

float linearizeDepth(float depth)
{
	// Convert non-linear depth buffer value to linear eye-space depth
	float z = depth * 2.0 - 1.0; // NDC
	return (2.0 * nearPlane * farPlane) / (farPlane + nearPlane - z * (farPlane - nearPlane));
}

void main()
{
	vec3 currentColor = texture(screenTexture, texCoords).rgb;
	float currentDepth = texture(depthTexture, texCoords).r;
	vec3 finalColor = currentColor;

	vec3 fogColor = vec3(0.53, 0.81, 0.92);

	// Scale fog with render distance
	float fogStart = renderDistance * 5.0;
	float fogEnd   = renderDistance * 15.0;

	// Linearize using provided near/far plane
	// float linearDepth = linearizeDepth(currentDepth);
	float near = 0.1;
	float far  = 9000.0;
	float z = currentDepth * 2.0 - 1.0;
	float linearDepth = (2.0 * near * far) / (far + near - z * (far - near));

	float fogFactor = clamp((linearDepth - fogStart) / (fogEnd - fogStart), 0.0, 1.0);
	fogFactor = smoothstep(0.0, 1.0, fogFactor);

	if (isUnderwater == 1)
	{
		// Depth-aware underwater fog with smooth tint ramp based on camera depth below water.
		float depthMeters = max(0.0, waterHeight - viewPos.y);
		float depthUnder  = smoothstep(UW_MIN_TINT_DEPTH, UW_MAX_TINT_DEPTH, depthMeters);
		// Color ramp from shallow aqua to deep blue as we descend
		vec3 shallowAqua = vec3(0.07, 0.24, 0.28);
		vec3 deepBlue    = vec3(0.00, 0.12, 0.25);
		vec3 uwTint      = mix(shallowAqua, deepBlue, depthUnder);

		float linDepthUW = linearizeDepth(currentDepth);

		// Limit the effective path length through water to avoid crushing distant details.
		// Slow the growth of effective path length with depth for gentler progression
		float depthRamp = pow(depthUnder, UW_DENSITY_EXP);
		float maxPath = mix(80.0, 320.0, depthRamp); // meters
		float effDepth = min(linDepthUW * UW_DISTANCE_SPEED, maxPath);

		// Softer density curve underwater so solids remain visible outside water.
		// Start 2x stronger at shallow depth, then increase more slowly with depth.
		float density = mix(UW_DENSITY_BASE * 2.0, UW_DENSITY_DEEP, depthRamp);
		float fog = 1.0 - exp(-density * effDepth);

		// For far away pixels, fog is attenuated so we can see outside blocks from in the water
		const float skyDepthStart = 0.995;   // begin attenuation
		const float skyDepthEnd   = 0.9999;  // fully attenuated at sky
		float skyRamp   = smoothstep(skyDepthStart, skyDepthEnd, currentDepth);
		float skyAtten  = mix(1.0, 0.9, skyRamp); // 0.6 was the old fixed cap
		fog *= skyAtten;

		// slight desaturation to simulate underwater scattering
		float gray = dot(finalColor, vec3(0.299, 0.587, 0.114));
		finalColor = mix(finalColor, vec3(gray), 0.12);

		// apply color shift with fog amount
		finalColor = mix(finalColor, uwTint, fog);
	}
	else
		finalColor = mix(finalColor, fogColor, fogFactor);
	FragColor = vec4(finalColor, 1.0);
}
