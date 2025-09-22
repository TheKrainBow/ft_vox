#version 430 core

in vec2 texCoords;
out vec4 FragColor;

uniform sampler2D screenTexture;
uniform sampler2D depthTexture;
uniform samplerCube skybox;

uniform mat4 invProjection;
uniform mat3 invViewRot;    // inverse of rotation-only view matrix
uniform float depthThreshold; // e.g. 0.9995
uniform int useLod0;          // 1 to force base LOD
uniform float edgeBias;       // pull away from face edges, e.g. 0.998

void main()
{
	vec3 sceneCol = texture(screenTexture, texCoords).rgb;
	float depth = texture(depthTexture, texCoords).r;

	// If geometry is present, keep scene color
	if (depth < depthThreshold) {
		FragColor = vec4(sceneCol, 1.0);
		return;
	}

	// Reconstruct view-space direction from NDC using inverse projection
	vec2 uv = texCoords * 2.0 - 1.0;
	vec4 clip = vec4(uv, 1.0, 1.0);
	vec4 view = invProjection * clip;
	vec3 dirVS = normalize(view.xyz);
	vec3 dirWS = normalize(invViewRot * dirVS) * edgeBias;

	vec3 skyCol = (useLod0 != 0) ? textureLod(skybox, dirWS, 0.0).rgb
									: texture(skybox, dirWS).rgb;
	FragColor = vec4(skyCol, 1.0);
}
