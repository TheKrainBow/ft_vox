#version 430
layout(local_size_x = 64) in;

struct DrawCmd { uint count, instanceCount, first, baseInstance; };

layout(std430, binding=0)  readonly  buffer PosResIn	{ vec4 posRes[]; };
layout(std430, binding=1)  readonly  buffer Templates	{ DrawCmd templ[]; };
layout(std430, binding=2)  writeonly buffer OutCmds		{ DrawCmd outCmds[]; };
layout(std430, binding=5)  coherent  buffer Counter		{ uint drawCount; };
layout(std430, binding=6)  writeonly buffer PosResOut	{ vec4 outPosRes[]; };

layout(std140, binding=3) uniform Frustum { vec4 planes[6]; };
uniform uint  numDraws;
uniform float chunkSize;

bool aabbOutsideFrustum(vec3 mn, vec3 mx) {
	vec3 c = 0.5*(mn+mx);
	vec3 e = 0.5*(mx-mn) + vec3(1.0); // conservative
	const float tol = 1.0;
	for (int k=0;k<6;++k) {
		vec3 n=planes[k].xyz; float d=planes[k].w;
		float r = e.x*abs(n.x)+e.y*abs(n.y)+e.z*abs(n.z);
		float s = dot(n,c)+d;
		if (s + r < -tol) return true;
	}
	return false;
}

void main() {
	uint i = gl_GlobalInvocationID.x;
	if (i >= numDraws) return;

	DrawCmd t = templ[i];

	vec3 mn = posRes[i].xyz;
	vec3 mx = mn + vec3(chunkSize);

	if (aabbOutsideFrustum(mn, mx) || t.instanceCount == 0u) {
		return; // culled
	}

	uint dst = atomicAdd(drawCount, 1u);
	outCmds[dst]   = t;						// compacted command
	outPosRes[dst] = vec4(mn, posRes[i].w);	// matching compacted metadata
}
