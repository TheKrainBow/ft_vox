#version 430 core
layout(location=0) in vec3 aPos;
uniform mat4 projection;
uniform mat4 view;
uniform vec3 cameraPos;
uniform vec3 worldOffset;
uniform float expand;
uniform float depthBias;

void main(){
	// Inflate the unit cube slightly so it sits just above faces
	vec3 s = sign(aPos - vec3(0.5));
	vec3 inflated = aPos + s * expand;

	vec3 worldP = worldOffset + inflated;
	vec3 camRel = worldP - cameraPos;

	vec4 clip = projection * view * vec4(camRel, 1.0);

	// Pull a tiny bit toward the camera to win depth ties
	clip.z -= depthBias * clip.w;

	gl_Position = clip;
}
