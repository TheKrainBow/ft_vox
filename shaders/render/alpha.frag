#version 430 core

uniform vec3 lightColor;
uniform int timeValue;
uniform mat4 view;
uniform vec3 cameraPos; // explicit camera world position
uniform sampler2DArray textureArray;
uniform bool showtrianglemesh; // wireframe mode toggle

in vec2 TexCoord;
flat in int TextureID;
in vec3 Normal;
in vec3 FragPos;

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

// Lighting helpers copied from terrain.frag
vec3 computeSunPosition(float time) {
	vec3 viewPos = cameraPos;
	const float pi = 3.14159265359;
	const float radius = 6000.0;
	const float dayStart = 42000.0;       // sunrise
	const float dayLen   = 86400.0 - dayStart; // 44400 (day duration)
	const float nightLen = dayStart;      // 42000 (night duration)

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

float calculateDiffuseLight(float time, vec3 lightDir) {
	vec3 norm = normalize(Normal);
	return max(dot(norm, lightDir), 0.0);
}

float calculateAmbientLight(float time) {
	const float pi = 3.14159265359;
	const float dayStart = 42000.0;
	const float dayLen   = 86400.0 - dayStart; // 44400
	float dayPhase = clamp((time - dayStart) / dayLen, 0.0, 1.0);
	float solar = sin(dayPhase * pi);
	float nightAmbient = 0.10;
	float dayAmbient   = 0.35;
	return mix(nightAmbient, dayAmbient, solar);
}

float calculateSpecularLight(float time, vec3 lightDir) {
	vec3 viewPos = cameraPos;
	vec3 norm = normalize(Normal);
	float specularStrength = 0.2;
	vec3 viewDir = normalize(viewPos - FragPos);
	vec3 reflectDir = reflect(-lightDir, norm);
	float spec = pow(max(dot(viewDir, reflectDir), 0.1), 0.8);
	float specular = specularStrength * spec;
	return specular;
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
	// Only leaves reach this shader, but keep guard
	if (TextureID != 11) discard;

	vec4 texColor = texture(textureArray, vec3(TexCoord, TextureID));
	// In wireframe mode, don't discard by alpha so edges remain visible
	if (!showtrianglemesh) {
		// Masked alpha: discard mostly transparent texels so depth behaves like opaque
		if (texColor.a < 0.5) discard;
	}

	if (fullBright) { FragColor = vec4(texColor.rgb * lightColor, 1.0); return; }
	vec3 lightDir = normalize(sunDir.y >= 0.0 ? sunDir : -sunDir);

	float ambient = calculateAmbientLight(timeValue);
	float diffuse = calculateDiffuseLight(timeValue, lightDir);
	float specular = calculateSpecularLight(timeValue, lightDir);

	float ambientWeight = 0.6;
	float diffuseWeight = 0.9;
	float specularWeight = 0.5;

	const float pi = 3.14159265359;
	const float dayStart = 42000.0;
	const float dayLen   = 86400.0 - dayStart;
	float dayPhase = clamp((timeValue - dayStart) / dayLen, 0.0, 1.0);
	float sun = sin(dayPhase * pi);
	sun = smoothstep(0.0, 0.15, sun);
	float moonAmt = moonLightStrength * smoothstep(0.0,0.15,-sunDir.y);
	float diffuseFactor = 0.2 * (sun+moonAmt);

	float visibility = 1.0 - computeShadow(FragPos,Normal,lightDir);
	float skyFill = mix(0.008,ambient * ambientWeight + 0.1,sun) + 0.15 * moonAmt;
	// A little scattered daylight softens surface shadows. Probe above low
	// occluders; a cave roof still blocks this fill. Direct sun stays shadowed.
	float surfaceFill = 0.0;
	if (visibility < 0.999 && sun > 0.0) {
		float openSky = 1.0 - computeShadow(FragPos + vec3(0,12,0),vec3(0,1,0),lightDir);
		surfaceFill = 0.14 * sun * (1.0-visibility) * openSky;
	}
	float totalLight = clamp(0.015 + surfaceFill + visibility * (skyFill +
		diffuse * diffuseWeight * diffuseFactor + specular * specularWeight * (sun+moonAmt)),0.0,1.0) + calculatePlayerLight(FragPos);

	vec3 result = totalLight * lightColor * texColor.rgb;
	FragColor = vec4(result, 1.0);
}
