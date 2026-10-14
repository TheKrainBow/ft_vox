#version 430 core
in vec2 texCoords;
out vec4 FragColor;
uniform sampler2D screenTexture;
uniform int isUnderwater;
uniform bool isInLava;

void main() {
	vec3 scene = texture(screenTexture, texCoords).rgb;
	if (isInLava) {
		vec3 color=mix(scene*vec3(0.65,0.12,0.025),vec3(0.62,0.07,0.008),0.90);
		float edge=smoothstep(0.15,0.70,length(texCoords-vec2(0.5)));
		FragColor=vec4(color*(1.0-0.20*edge),1.0);
		return;
	}
	if (isUnderwater != 1) {
		FragColor = vec4(scene, 1.0);
		return;
	}
	// Depth-independent immersion cue: blue/green absorption over the entire
	// screen, plus a soft vignette. Visible even beside a wall in a shallow pool.
	vec3 submerged = scene * vec3(0.38, 0.72, 0.90);
	submerged = mix(submerged, vec3(0.0, 0.055, 0.09), 0.12);
	float edge = smoothstep(0.15, 0.70, length(texCoords - vec2(0.5)));
	submerged *= 1.0 - 0.25 * edge;
	FragColor = vec4(submerged, 1.0);
}
