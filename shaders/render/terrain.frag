#version 430 core

uniform vec3 lightColor;
uniform int  timeValue;
uniform mat4 view;
uniform vec3 cameraPos;
uniform sampler2DArray textureArray;
uniform sampler2DShadow shadowMap;
uniform mat4 lightSpaceMatrix;
uniform vec3 sunDir;
uniform float shadowTexelWorld;
uniform float shadowNear;
uniform float shadowFar;
uniform float shadowBiasSlope;
uniform float shadowBiasConstant;

in vec2 TexCoord;
flat in int TextureID;
in vec3 Normal;
in vec3 FragPos;

out vec4 FragColor;

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

float computeShadow(vec3 fragPos, vec3 normal, vec3 lightDir)
{
	vec4 fragPosLightSpace = lightSpaceMatrix * vec4(fragPos, 1.0);
	vec3 proj = fragPosLightSpace.xyz / fragPosLightSpace.w;
	proj = proj * 0.5 + 0.5;

	if (proj.z > 1.0 || proj.x < 0.0 || proj.x > 1.0 || proj.y < 0.0 || proj.y > 1.0)
		return 0.0;

	vec3 n = normalize(normal);
	vec3 l = normalize(lightDir);
	float ndotl = max(dot(n, l), 0.0);
	if (ndotl <= 0.0)
		return 0.0;

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
    vec4 texColor = texture(textureArray, vec3(TexCoord, TextureID));
    vec3 lightDir = normalize(sunDir);
    vec3 norm = normalize(Normal);

    float ambient  = calculateAmbientLight(timeValue);
    float diffuse  = max(dot(norm, lightDir), 0.0);
    float specular = calculateSpecularLight(timeValue, lightDir);

	const float pi = 3.14159265359;
	const float dayStart = 42000.0;
	const float dayLen   = 86400.0 - dayStart;
	float dayPhase = clamp((timeValue - dayStart) / dayLen, 0.0, 1.0);
	float sunAmt = smoothstep(0.0, 0.15, sin(dayPhase * pi));
	float diffuseFactor = 0.2 * sunAmt;

	float shadow = computeShadow(FragPos, norm, lightDir);
	float shadowFactor = 1.0 - shadow;

	float totalLight = clamp(ambient * 0.6 +
						   diffuse * 0.9 * diffuseFactor * shadowFactor +
						   specular * 0.5 * shadowFactor + 0.1, 0.0, 1.0);

    vec3 result = totalLight * lightColor * texColor.rgb;
    FragColor = vec4(result, texColor.a);
}
