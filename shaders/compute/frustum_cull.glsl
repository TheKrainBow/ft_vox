#version 430
layout(local_size_x = 64) in;

struct DrawCmd { uint count, instanceCount, first, baseInstance; };

layout(std430, binding=0)  readonly  buffer PosResIn	{ vec4 posRes[]; };
layout(std430, binding=1)  readonly  buffer Templates	{ DrawCmd templ[]; };
layout(std430, binding=2)  writeonly buffer OutCmds		{ DrawCmd outCmds[]; };
layout(std430, binding=5)  coherent  buffer Counter		{ uint drawCount; };
layout(std430, binding=6)  writeonly buffer PosResOut	{ vec4 outPosRes[]; };
layout(std430, binding=7)  readonly  buffer MetaIn      { uint metaIn[]; };
layout(std430, binding=8)  writeonly buffer MetaOut     { uint metaOut[]; };

layout(std140, binding=3) uniform Frustum { vec4 planes[6]; };

// Optional occlusion inputs (previous-frame depth)
uniform bool  useOcclusion;
uniform bool  debugLogOcclusion; // when true, record IDs culled by Hi-Z
uniform uint  hystThreshold;     // frames to wait before actually culling
uniform uint  revealThreshold;   // frames to keep visible after reveal
uniform sampler2D depthTex;
uniform mat4 view;
uniform mat4 proj;
uniform vec2 viewport; // pixels
uniform vec3 camPos;   // world-space camera position
uniform float chunkSize;
uniform uint  numDraws;

// Debug output buffers (only used when debugLogOcclusion == true)
layout(std430, binding=9)  writeonly buffer CullDebugIDs { uint culledIDs[]; };
layout(std430, binding=10) coherent  buffer CullDebugCount { uint culledCount; };
// Temporal hysteresis state (per draw)
layout(std430, binding=11) buffer Hysteresis { uint hyst[]; };
// Reveal-hold state (per draw)
layout(std430, binding=12) buffer RevealHold { uint reveal[]; };

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

// Project a point to NDC [0,1] screen and depth [0,1]. Returns false if behind camera.
bool projectPoint(vec3 p, out vec2 uv01, out float depth01) {
	vec4 clip = proj * view * vec4(p, 1.0);
	if (clip.w <= 0.0) return false; // behind eye
	vec3 ndc = clip.xyz / clip.w;            // [-1,1]
	uv01 = ndc.xy * 0.5 + 0.5;               // [0,1]
	depth01 = ndc.z * 0.5 + 0.5;             // [0,1]
	return true;
}

bool aabbOccluded(vec3 mn, vec3 mx) {
	if (!useOcclusion) return false; // cannot test -> keep visible

	// 8 corners of the box
	vec3 c[8];
	c[0]=vec3(mn.x,mn.y,mn.z); c[1]=vec3(mx.x,mn.y,mn.z);
	c[2]=vec3(mn.x,mx.y,mn.z); c[3]=vec3(mx.x,mx.y,mn.z);
	c[4]=vec3(mn.x,mn.y,mx.z); c[5]=vec3(mx.x,mn.y,mx.z);
	c[6]=vec3(mn.x,mx.y,mx.z); c[7]=vec3(mx.x,mx.y,mx.z);

	vec2 uvMin = vec2( 1e9);
	vec2 uvMax = vec2(-1e9);
	float boxNear = 1.0;
	bool anyProjected = false;
	for (int i=0;i<8;++i) {
		vec2 uv; float dz;
		if (!projectPoint(c[i], uv, dz)) continue; // skip behind-eye corners
		anyProjected = true;
		uvMin = min(uvMin, uv);
		uvMax = max(uvMax, uv);
		boxNear = min(boxNear, dz);
	}
	// Compute a better front depth using closest point from camera to AABB
	vec3 pClosest = clamp(camPos, mn, mx);
	vec2 uvC; float dC;
	if (projectPoint(pClosest, uvC, dC)) {
		boxNear = min(boxNear, dC);
		uvMin = min(uvMin, uvC);
		uvMax = max(uvMax, uvC);
		anyProjected = true;
	}

	if (!anyProjected) return false; // unknown, keep

	// If camera is inside the AABB, skip occlusion to avoid self-occlusion flicker
	if (camPos.x >= mn.x && camPos.x <= mx.x &&
		camPos.y >= mn.y && camPos.y <= mx.y &&
		camPos.z >= mn.z && camPos.z <= mx.z) {
		return false;
	}

    // Very near AABB: skip occlusion (avoid corner adjacency popping and flicker)
    float d2 = dot(camPos - pClosest, camPos - pClosest);
    if (d2 < 16.0) { // within ~4 blocks
        return false;
    }

	// Clamp rect to viewport
	vec2 pxMinF = floor(clamp(uvMin * viewport, vec2(0.0), viewport - vec2(1.0)));
	vec2 pxMaxF = ceil (clamp(uvMax * viewport, vec2(0.0), viewport - vec2(1.0)));
    // Add a wider safety border to stabilize around thin occluders
    ivec2 pxMin = ivec2(pxMinF) - ivec2(6);
    ivec2 pxMax = ivec2(pxMaxF) + ivec2(6);
    pxMin = clamp(pxMin, ivec2(0), ivec2(viewport) - ivec2(1));
    pxMax = clamp(pxMax, ivec2(0), ivec2(viewport) - ivec2(1));
    if (pxMax.x < pxMin.x || pxMax.y < pxMin.y) return false; // empty

    // Skip occlusion for very small projected areas (highly unstable temporally)
    ivec2 extent = pxMax - pxMin + ivec2(1);
    int areaPx = extent.x * extent.y;
    if (areaPx < 64) return false;

	// Coarse sampling grid (reduced to lighten compute cost)
    const int STEPS = 5;
    float eps = 1.5e-1; // larger tolerance on depth compare (reduce popping)
	for (int iy = 0; iy < STEPS; ++iy) {
		for (int ix = 0; ix < STEPS; ++ix) {
			float tx = (float(ix) + 0.5) / float(STEPS);
			float ty = (float(iy) + 0.5) / float(STEPS);
			vec2 pxf = mix(vec2(pxMin) + vec2(0.5), vec2(pxMax) + vec2(0.5), vec2(tx, ty));
			ivec2 ip = ivec2(clamp(pxf, vec2(0.5), viewport - vec2(0.5)));
			float d = texelFetch(depthTex, ip, 0).r;
			// If any sample depth is not strictly in front of the box near plane, not fully occluded
			if (d >= boxNear - eps) return false;
		}
	}
	return true; // all samples closer than the nearest part of the box -> occluded
}

void main() {
	uint i = gl_GlobalInvocationID.x;
	if (i >= numDraws) return;

	DrawCmd t = templ[i];
	if (t.instanceCount == 0u) return; // nothing to draw

	vec3 mn = posRes[i].xyz;
	vec3 mx = mn + vec3(chunkSize);

	// Frustum test (conservative)
	if (aabbOutsideFrustum(mn, mx)) return;

    // Optional previous-frame occlusion test with temporal hysteresis
    bool occl = aabbOccluded(mn, mx);
    if (useOcclusion) {
        uint h = hyst[i];
        uint rv = reveal[i];

        // If just became visible (occl=false), start/refresh a reveal hold
        if (!occl) {
            uint target = max(1u, revealThreshold);
            if (rv < target) reveal[i] = target;
            // Decay hysteresis so it doesn't quickly re-trigger cull
            hyst[i] = (h > 0u) ? (h - 1u) : 0u;
        } else {
            // If in reveal hold window, force visible and decrement hold
            if (rv > 0u) {
                reveal[i] = rv - 1u;
                occl = false;
            }
        }

        if (occl) {
            // Increase hysteresis; only cull when it reaches the threshold
            uint nh = (h < 0xFFFFFFFFu) ? (h + 1u) : h;
            hyst[i] = nh;
            bool allowCull = (nh >= max(1u, hystThreshold));
            if (allowCull) {
                if (debugLogOcclusion) {
                    uint idx = atomicAdd(culledCount, 1u);
                    if (idx < numDraws) culledIDs[idx] = i;
                }
                return;
            }
            // Not yet allowed to cull: treat as visible this frame
        }
    }

	// Visible: append to compacted command/metadata buffers
	uint dst = atomicAdd(drawCount, 1u);
	outCmds[dst]   = t;
	outPosRes[dst] = vec4(mn, posRes[i].w);
	metaOut[dst]   = metaIn[i];
}
