#version 430 core
in vec2 texCoords;
out vec4 FragColor;
uniform mat4 invProjection;
uniform mat3 invViewRot;
uniform vec3 skyTopColor;
uniform vec3 skyHorizonColor;

void main() {
	vec4 ray = invProjection * vec4(texCoords*2.0-1.0,1.0,1.0);
	vec3 direction = normalize(invViewRot * ray.xyz);
	float height = pow(clamp(direction.y,0.0,1.0),0.55);
	FragColor = vec4(mix(skyHorizonColor,skyTopColor,height),1.0);
}
