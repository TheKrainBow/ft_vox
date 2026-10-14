#version 460 core

in vec2 vUV;
flat in int vType;
in float vHeightMask;
in vec3 vWorldPos;

out vec4 FragColor;

uniform vec3 playerLightPos;
uniform float playerLightRadius;
uniform bool playerLightEnabled;
uniform bool fullBright;
uniform sampler2DShadow shadowMap;
uniform mat4 lightSpaceMatrix;
uniform vec3 sunDir;
uniform float moonLightStrength;
uniform float shadowTexelWorld;
uniform float shadowNear;
uniform float shadowFar;
uniform float shadowBiasSlope;
uniform float shadowBiasConstant;

float calculatePlayerLight(vec3 worldPos) {
	if (!playerLightEnabled || playerLightRadius <= 0.0) return 0.0;
	// Fade gently across the full radius, with a smooth slope at both ends.
	float distanceToPlayer = distance(worldPos, playerLightPos);
	return 0.5 * (1.0 - smoothstep(0.0, playerLightRadius, distanceToPlayer));
}

uniform sampler2DArray flowerTex;
uniform int shortGrassLayer; // layer index for short_grass (greyscale), -1 if not present
uniform vec3  cameraPos;
uniform int   timeValue;
uniform vec3  lightColor;

vec3 computeSunPosition(float time) {
	vec3 viewPos = cameraPos;
	const float pi = 3.14159265359;
	const float radius = 6000.0;
	const float dayStart = 42000.0;
	const float dayLen   = 86400.0 - dayStart;
	const float nightLen = dayStart;
	float angle;
	if (time < dayStart) {
		float phase = clamp(time / nightLen, 0.0, 1.0);
		angle = pi + phase * pi;
	} else {
		float phase = clamp((time - dayStart) / dayLen, 0.0, 1.0);
		angle = phase * pi;
	}
	float x = radius * cos(angle);
	float y = radius * sin(angle);
	float z = 0.0f;
	return viewPos + vec3(x, y, z);
}

float calculateAmbientLight(float time) {
	const float pi = 3.14159265359;
	const float dayStart = 42000.0;
	const float dayLen   = 86400.0 - dayStart;
	float dayPhase = clamp((time - dayStart) / dayLen, 0.0, 1.0);
	float solar = sin(dayPhase * pi);
	float nightAmbient = 0.10;
	float dayAmbient   = 0.35;
	return mix(nightAmbient, dayAmbient, solar);
}

float computeShadow(vec3 fragPos, vec3 normal, vec3 lightDir)
{
	vec4 fragPosLightSpace = lightSpaceMatrix * vec4(fragPos, 1.0);
	vec3 proj = fragPosLightSpace.xyz / fragPosLightSpace.w;
	proj = proj * 0.5 + 0.5;

	if (proj.z < 0.0 || proj.z > 1.0 || proj.x < 0.0 || proj.x > 1.0 || proj.y < 0.0 || proj.y > 1.0)
		return 1.0;

	vec3 n = normalize(normal);
	vec3 l = normalize(lightDir);
	float ndotl = max(dot(n, l), 0.0);

	float biasWorld = shadowBiasConstant + shadowBiasSlope * (1.0 - ndotl);
	biasWorld = max(biasWorld * shadowTexelWorld, 0.0005);
	float biasDepth = biasWorld / max(shadowFar - shadowNear, 1e-4);

	vec2 texelSize = 1.0 / vec2(textureSize(shadowMap, 0));
	float samples = 0.0;
	for (int x = -1; x <= 1; ++x)
	for (int y = -1; y <= 1; ++y) {
		vec2 offset = vec2(float(x), float(y)) * texelSize;
		samples += 1.0 - texture(shadowMap, vec3(proj.xy + offset, proj.z - biasDepth));
	}
	return samples / 9.0;
}

void main() {
	vec4 col = texture(flowerTex, vec3(vUV, float(vType)));
	// Harden alpha cutout a bit to eliminate fringe pixels from semi-transparent sources
	if (col.a < 0.6) discard;

	// If this is the short grass layer, tint grayscale to green
	if (shortGrassLayer >= 0 && vType == shortGrassLayer) {
		// Use the red channel as intensity (greyscale source)
		float g = col.r;
		vec3 grass = vec3(0.38, 0.76, 0.36); // pleasant grass green
		col.rgb = grass * g;
	}

	// Upward facing diffuse to mimic ground-lit plants
	vec3 lightDir = normalize(sunDir.y >= 0.0 ? sunDir : -sunDir);
	float ambient = calculateAmbientLight(float(timeValue));
	float sun = max(lightDir.y, 0.0); // cosine vs up vector
	// Match terrain day scaling a bit
	const float pi = 3.14159265359;
	const float dayStart = 42000.0;
	const float dayLen   = 86400.0 - dayStart;
	float dayPhase = clamp((float(timeValue) - dayStart) / dayLen, 0.0, 1.0);
	float sunPhase = smoothstep(0.0, 0.15, sin(dayPhase * pi));
	float moonAmt = moonLightStrength * smoothstep(0.0,0.15,-sunDir.y);
	float diffuse = sun * 0.2 * (sunPhase+moonAmt);
	float visibility = 1.0 - computeShadow(vWorldPos,vec3(0,1,0),lightDir);
	float skyFill = mix(0.008,ambient * 0.6 + 0.1,sunPhase) + 0.15 * moonAmt;
	// A little scattered daylight softens surface shadows. Probe above low
	// occluders; a cave roof still blocks this fill. Direct sun stays shadowed.
	float surfaceFill = 0.0;
	if (visibility < 0.999 && sunPhase > 0.0) {
		float openSky = 1.0 - computeShadow(vWorldPos + vec3(0,12,0),vec3(0,1,0),lightDir);
		surfaceFill = 0.14 * sunPhase * (1.0-visibility) * openSky;
	}
	float total = fullBright ? 1.0 : clamp(0.015 + surfaceFill + visibility * (skyFill + diffuse * 0.9),0.0,1.0) + calculatePlayerLight(vWorldPos);

	float ao = mix(0.9, 1.0, vHeightMask);
	vec3 rgb = col.rgb * ao * lightColor * total;
	FragColor = vec4(rgb, 1.0);
}
