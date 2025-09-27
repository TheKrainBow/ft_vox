#version 430 core

uniform vec3 lightColor;
uniform int  timeValue;
uniform mat4 view;
uniform vec3 cameraPos;
uniform sampler2DArray textureArray;
// Cascaded shadow maps: near = 4096, then halved every 6 chunks
const int MAX_CASCADES = 12;
uniform sampler2DShadow shadowMaps[MAX_CASCADES];
uniform mat4 lightSpaceMatrices[MAX_CASCADES];
uniform vec3 sunDir;                     // directional light (world)
uniform vec3 shadowCenter;               // snapped center used for cascades
uniform float shadowTexelWorlds[MAX_CASCADES]; // world units per texel per cascade
uniform float lightNears[MAX_CASCADES];        // light ortho near per cascade
uniform float lightFars[MAX_CASCADES];         // light ortho far per cascade
uniform int   cascadeCount;                     // active cascades
uniform float cascadeRadii[MAX_CASCADES];       // ring radii (world units)

in vec2 TexCoord;
flat in int TextureID;
in vec3 Normal;
in vec3 FragPos;

out vec4 FragColor;

int pickCascade(vec3 fragPos, vec3 camPos)
{
	// Choose ring by XZ distance to the cascade center actually used to build the maps.
	// This stabilizes selection when the camera moves within a snapped grid cell.
	float d = length(fragPos.xz - shadowCenter.xz);
	for (int i = 0; i < cascadeCount; ++i) {
		if (d <= cascadeRadii[i]) return i;
	}
	return cascadeCount - 1;
}

// Blend factor between cascade i and i+1 near the boundary to avoid flashing
float cascadeBlendFactor(int i, vec3 fragPos, vec3 camPos)
{
	if (i >= cascadeCount - 1) return 0.0; // no next cascade
	float d = length(fragPos.xz - shadowCenter.xz);
	float R = cascadeRadii[i];
	// Use a blend width of ~2-3 shadow texels in world units (max of the two cascades)
	float w = 4.0 * max(shadowTexelWorlds[i], shadowTexelWorlds[i+1]);
	return smoothstep(R - w, R + w, d);
}

// Variable-cost PCF: heavier on near cascades, cheaper on far ones
float shadowPCF(int idx, vec4 fragPosLightSpace, vec3 normal, vec3 lightDir)
{
	// light-space -> NDC -> [0,1]
	vec3 proj = fragPosLightSpace.xyz / fragPosLightSpace.w;
	proj = proj * 0.5 + 0.5;

	// outside light frustum
	if (proj.z > 1.0 || proj.x < 0.0 || proj.x > 1.0 || proj.y < 0.0 || proj.y > 1.0)
		return 0.0;

	// slope-scaled receiver bias in WORLD units (stable across frustum sizes)
	float ndotl = max(dot(normalize(normal), normalize(lightDir)), 0.0);
	float slope = 1.0 - ndotl;                         // 0 facing light, 1 grazing
	// Slightly increase bias for far cascades to fight precision loss
	float farScale = (cascadeCount > 1) ? (1.0 + 0.5 * float(idx) / float(max(cascadeCount - 1, 1))) : 1.0;
	float biasWorld = max(0.5 * shadowTexelWorlds[idx] * farScale, 0.0005);
	float biasDepth = biasWorld / max(lightFars[idx] - lightNears[idx], 1e-4);
	biasDepth *= (0.35 + 0.65 * slope);
	vec2 texelSize = 1.0 / vec2(textureSize(shadowMaps[idx], 0));

	// Early-out for backfacing surfaces relative to light: shadowing has no visible effect
	if (ndotl <= 0.0) return 0.0;

	// Near cascade: 3x3 PCF for quality
	if (idx == 0) {
		float s = 0.0;
		for (int x = -1; x <= 1; ++x)
		for (int y = -1; y <= 1; ++y) {
			vec2 uv = proj.xy + vec2(x, y) * texelSize;
			s += 1.0 - texture(shadowMaps[idx], vec3(uv, proj.z - biasDepth));
		}
		return s / 9.0;
	}

	// Mid cascade: 4-tap PCF (Poisson-ish) to cut cost
	if (idx == 1) {
		const vec2 offs[4] = vec2[4](
			vec2(-0.5,  0.0),
			vec2( 0.5,  0.0),
			vec2( 0.0, -0.5),
			vec2( 0.0,  0.5)
		);
		float s = 0.0;
		for (int i = 0; i < 4; ++i) {
			vec2 uv = proj.xy + offs[i] * texelSize * 1.0;
			s += 1.0 - texture(shadowMaps[idx], vec3(uv, proj.z - biasDepth));
		}
		return s * 0.25;
	}

	// Far cascades: rely on hardware 2x2 PCF via linear filtering (single sample)
	return 1.0 - texture(shadowMaps[idx], vec3(proj.xy, proj.z - biasDepth));
}

float calculateDiffuseLight(float time, vec3 lightDir) {
	vec3 norm = normalize(Normal);
	return max(dot(norm, lightDir), 0.0);
}
float calculateAmbientLight(float time) {
	const float pi = 3.14159265359;
	const float dayStart = 42000.0;
	const float dayLen   = 86400.0 - dayStart;
	float dayPhase = clamp((time - dayStart) / dayLen, 0.0, 1.0);
	float solar = sin(dayPhase * pi);
	return mix(0.10, 0.35, solar);
}
float calculateSpecularLight(float time, vec3 lightDir) {
	vec3 norm = normalize(Normal);
	float specularStrength = (TextureID == 6) ? 0.1 : 0.2;
	vec3 viewDir = normalize(cameraPos - FragPos);
	vec3 reflectDir = reflect(-lightDir, norm);
	float spec = pow(max(dot(viewDir, reflectDir), 0.1), 0.8);
	return specularStrength * spec;
}

void main() {
	vec4 texColor = texture(textureArray, vec3(TexCoord, TextureID));
	vec3 lightDir = normalize(sunDir);
	vec3 norm = normalize(Normal);
	int cascadeIdx = pickCascade(FragPos, cameraPos);
	int nextIdx    = min(cascadeIdx + 1, cascadeCount - 1);
	vec4 fragPosLightSpace0 = lightSpaceMatrices[cascadeIdx] * vec4(FragPos, 1.0);
	vec4 fragPosLightSpace1 = lightSpaceMatrices[nextIdx]    * vec4(FragPos, 1.0);

	float ambient  = calculateAmbientLight(timeValue);
	float diffuse  = max(dot(norm, lightDir), 0.0);
	float specular = calculateSpecularLight(timeValue, lightDir);

	// daylight modulation for diffuse
	const float pi = 3.14159265359;
	const float dayStart = 42000.0;
	const float dayLen   = 86400.0 - dayStart;
	float dayPhase = clamp((timeValue - dayStart) / dayLen, 0.0, 1.0);
	float sunAmt = smoothstep(0.0, 0.15, sin(dayPhase * pi));
	float diffuseFactor = 0.2 * sunAmt;

	float f = cascadeBlendFactor(cascadeIdx, FragPos, cameraPos);
	float s0 = shadowPCF(cascadeIdx, fragPosLightSpace0, norm, lightDir);
	float shadow = s0;
	if (f > 0.001) {
		float s1 = shadowPCF(nextIdx, fragPosLightSpace1, norm, lightDir);
		shadow = mix(s0, s1, f);
	}
	float shadowFactor = 1.0 - shadow;

	float totalLight =
		clamp(ambient * 0.6 +
				diffuse * 0.9 * diffuseFactor * shadowFactor +
				specular * 0.5 * shadowFactor, 0.0, 1.0);

	vec3 result = totalLight * lightColor * texColor.rgb;
	FragColor = vec4(result, texColor.a);
}
