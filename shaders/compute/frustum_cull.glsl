#version 430
layout(local_size_x = 64) in;

struct DrawCmd { uint count, instanceCount, first, baseInstance; };

layout(std430, binding=0) readonly buffer PosRes    { vec4 posRes[]; }; // xyz = subchunk origin
layout(std430, binding=1) readonly buffer Templates { DrawCmd templ[]; };
layout(std430, binding=2) writeonly buffer OutCmds  { DrawCmd outCmds[]; };

layout(std140, binding=3) uniform Frustum { vec4 planes[6]; };

uniform uint  numDraws;
uniform float chunkSize;

// --- set to 1 to bypass culling and just copy (debug) ---
#define BYPASS_CULL 0

void main() {
	uint i = gl_GlobalInvocationID.x;
	if (i >= numDraws) return;

	DrawCmd t = templ[i];
	DrawCmd o = t;

#if BYPASS_CULL
	outCmds[i] = o;
	return;
#endif

	// AABB: subchunk origin + cube of chunkSize
	vec3 mn = posRes[i].xyz;
	vec3 mx = mn + vec3(chunkSize);

	// --- conservative expansion to avoid borderline pops ---
	// expand by ~1 block; tweak if you still see edge flicker
	const float expand = 1.0;
	vec3 c = 0.5 * (mn + mx);
	vec3 e = 0.5 * (mx - mn) + vec3(expand);

	// use a small negative tolerance: only cull if *definitely* outside
	const float tol = 1.0; // in “distance units”; 1–2 blocks is usually enough

	bool culled = false;
	for (int k = 0; k < 6; ++k) {
		vec3  n = planes[k].xyz;
		float d = planes[k].w;
		float r = e.x * abs(n.x) + e.y * abs(n.y) + e.z * abs(n.z);
		float s = dot(n, c) + d;
		if (s + r < -tol) { culled = true; break; }
	}

	o.instanceCount = culled ? 0u : t.instanceCount;
	outCmds[i] = o;
}
