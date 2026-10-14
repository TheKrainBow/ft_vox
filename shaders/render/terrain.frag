#version 430 core

uniform vec3 lightColor;
uniform int  timeValue;
uniform mat4 view;
uniform vec3 cameraPos;
uniform sampler2DArray textureArray;
uniform sampler2DShadow shadowMap;
uniform mat4 lightSpaceMatrix;
uniform vec3 sunDir;
uniform float moonLightStrength;
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

uniform vec3 playerLightPos;
uniform float playerLightRadius;
uniform bool playerLightEnabled;
uniform bool fullBright;

float calculatePlayerLight(vec3 worldPos) {
	if (!playerLightEnabled || playerLightRadius <= 0.0) return 0.0;
	// Fade gently across the full radius, with a smooth slope at both ends.
	float distanceToPlayer = distance(worldPos, playerLightPos);
	return 0.5 * (1.0 - smoothstep(0.0, playerLightRadius, distanceToPlayer));
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

// Sparse world-space light field rebuilt when lava geometry changes. Entries
// point to exposed face centers; falloff is evaluated at the actual fragment.
layout(std430,binding=13) readonly buffer LavaLightCells { ivec4 lavaCells[]; };
layout(std430,binding=14) readonly buffer LavaLightEmitters { vec4 lavaFaces[]; };
uniform int lavaLightMask;
uint lavaHash(ivec3 p) {
	uint h=uint(p.x)*73856093u ^ uint(p.y)*19349663u ^ uint(p.z)*83492791u;
	h^=h>>16; h*=0x7feb352du; h^=h>>15;
	return h;
}
int lavaEmitterAt(ivec3 p) {
	uint index=lavaHash(p)&uint(lavaLightMask);
	for (int probe=0;probe<16;++probe) {
		ivec4 cell=lavaCells[index];
		if (cell.w==0) return -1;
		if (all(equal(cell.xyz,p))) return cell.w-1;
		index=(index+1u)&uint(lavaLightMask);
	}
	return -1;
}
vec3 lavaRadiance(vec3 worldPos) {
	if (lavaLightMask<=0) return vec3(0.0);
	ivec3 base=ivec3(floor(worldPos-vec3(0.5)));
	float light=0.0;
	for (int z=0;z<2;++z) for (int y=0;y<2;++y) for (int x=0;x<2;++x) {
		int emitter=lavaEmitterAt(base+ivec3(x,y,z));
		if (emitter<0) continue;
		float d=distance(worldPos,lavaFaces[emitter].xyz);
		light=max(light,0.85*(1.0-smoothstep(0.0,4.0,d)));
	}
	return vec3(1.0,0.28,0.04)*light;
}

void main() {
	vec4 texColor = texture(textureArray, vec3(TexCoord, TextureID));
	if (fullBright) { FragColor = vec4(texColor.rgb * lightColor, texColor.a); return; }
	vec3 lightDir = normalize(sunDir.y >= 0.0 ? sunDir : -sunDir);
	vec3 norm = normalize(Normal);

	float ambient  = calculateAmbientLight(timeValue);
	float diffuse  = max(dot(norm, lightDir), 0.0);
	float specular = calculateSpecularLight(timeValue, lightDir);

	const float pi = 3.14159265359;
	const float dayStart = 42000.0;
	const float dayLen   = 86400.0 - dayStart;
	float dayPhase = clamp((timeValue - dayStart) / dayLen, 0.0, 1.0);
	float sunAmt = smoothstep(0.0, 0.15, sin(dayPhase * pi));
	float moonAmt = moonLightStrength * smoothstep(0.0,0.15,-sunDir.y);
	float diffuseFactor = 0.2 * (sunAmt+moonAmt);

	float playerLight = calculatePlayerLight(FragPos);
	float shadow = computeShadow(FragPos, norm, lightDir);
	// Occlusion affects the sky fill as well as diffuse/specular sunlight.
	// The remaining ambient floor is deliberately faint in caves and at night.
	float visibility = 1.0 - shadow;
	float skyFill = mix(0.008, ambient * 0.6 + 0.1, sunAmt) + 0.15 * moonAmt;
	// A little scattered daylight softens surface shadows. Probe above low
	// occluders; a cave roof still blocks this fill. Direct sun stays shadowed.
	float surfaceFill = 0.0;
	if (visibility < 0.999 && sunAmt > 0.0) {
		float openSky = 1.0 - computeShadow(FragPos + vec3(0,12,0),vec3(0,1,0),lightDir);
		surfaceFill = 0.14 * sunAmt * (1.0-visibility) * openSky;
	}
	float totalLight = clamp(0.015 + surfaceFill + visibility * (skyFill +
		diffuse * 0.9 * diffuseFactor + specular * 0.5 * (sunAmt+moonAmt)),0.0,1.0) + playerLight;

	vec3 result = (totalLight * lightColor + lavaRadiance(FragPos)) * texColor.rgb;
	FragColor = vec4(result, texColor.a);
}
