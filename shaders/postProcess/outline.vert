#version 430 core
layout(location=0) in vec3 aPos;
uniform mat4 projection;
uniform mat4 view;
uniform vec3 cameraPos;
uniform vec3 worldOffset;
uniform vec3 scale;
uniform float expand;
uniform float depthBias;

void main(){
	// Outward inflate per face (away from center)
	vec3 sgn = sign(aPos - vec3(0.5));
	vec3 worldP = worldOffset + aPos * scale + sgn * expand;

	vec3 camRel = worldP - cameraPos;
	vec4 clip   = projection * view * vec4(camRel, 1.0);
	clip.z -= depthBias * clip.w;   // depth bias to win ties
	gl_Position = clip;
}
