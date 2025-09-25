#version 430 core

uniform vec3 lightColor;
uniform int  timeValue;
uniform mat4 view;
uniform vec3 cameraPos;
uniform sampler2DArray textureArray;
uniform sampler2DShadow shadowMap;       // FIX: shadow sampler
uniform mat4 lightSpaceMatrix;
uniform vec3 sunDir;                     // directional light (world)
uniform float shadowTexelWorld;          // world units per shadow texel (set from CPU)
uniform float lightNear;        // light ortho near
uniform float lightFar;         // light ortho far

in vec2 TexCoord;
flat in int TextureID;
in vec3 Normal;
in vec3 FragPos;

out vec4 FragColor;

float shadowPCF(vec4 fragPosLightSpace, vec3 normal, vec3 lightDir)
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
	float biasWorld = max(0.5 * shadowTexelWorld, 0.0005);
	float biasDepth = biasWorld / max(lightFar - lightNear, 1e-4);
	biasDepth *= (0.35 + 0.65 * slope);
	vec2 texel = 1.0 / vec2(textureSize(shadowMap, 0));
	vec2 texelSize = 1.0 / vec2(textureSize(shadowMap, 0));
	float shadow = 0.0;
	for (int x = -1; x <= 1; ++x)
	for (int y = -1; y <= 1; ++y) {
		vec2 uv = proj.xy + vec2(x, y) * texelSize;
		// sampler2DShadow returns visibility (1=lit, 0=shadowed)
		shadow += 1.0 - texture(shadowMap, vec3(uv, proj.z - biasDepth));
	}
	return shadow /= 9.0;
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
	vec4 fragPosLightSpace = lightSpaceMatrix * vec4(FragPos, 1.0);

	float ambient  = calculateAmbientLight(timeValue);
	float diffuse  = calculateDiffuseLight(timeValue, lightDir);
	float specular = calculateSpecularLight(timeValue, lightDir);

	// daylight modulation for diffuse
	const float pi = 3.14159265359;
	const float dayStart = 42000.0;
	const float dayLen   = 86400.0 - dayStart;
	float dayPhase = clamp((timeValue - dayStart) / dayLen, 0.0, 1.0);
	float sunAmt = smoothstep(0.0, 0.15, sin(dayPhase * pi));
	float diffuseFactor = 0.2 * sunAmt;

	float shadow = shadowPCF(fragPosLightSpace, Normal, lightDir);
	float shadowFactor = 1.0 - shadow;

	float totalLight =
		clamp(ambient * 0.6 +
				diffuse * 0.9 * diffuseFactor * shadowFactor +
				specular * 0.5 * shadowFactor, 0.0, 1.0);

	vec3 result = totalLight * lightColor * texColor.rgb;
	FragColor = vec4(result, texColor.a);
}
