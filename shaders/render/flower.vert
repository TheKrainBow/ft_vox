#version 460 core

layout(location = 0) in vec3 aPos;   // base mesh position (unit)
layout(location = 1) in vec2 aUV;    // base mesh UV (0..1)

// Instance attributes
layout(location = 2) in vec4 iPosRot;      // xyz = block/world pos (meters), w = rot radians
layout(location = 3) in vec2 iScaleHeights; // x = scale jitter (0.9..1.1), y = heightScale (1 or 2)
layout(location = 4) in int  iType;        // flower type (texture layer)

uniform mat4 model;
uniform mat4 view;        // rotation-only view (camera-relative)
uniform mat4 projection;
uniform vec3 cameraPos;   // world-space camera position
uniform float time;       // seconds
uniform int   timeValue;  // world time (day/night like terrain)

out vec2 vUV;
flat out int vType;
out float vHeightMask;    // for wind sway: 0 at base, 1 at top
out vec3 vWorldPos;

void main() {
    float scale = iScaleHeights.x;
    float hscale = iScaleHeights.y; // 1.0 or 2.0 for tall plants

    // Rotate around Y (jitter) + build X-shaped cross from base mesh
    float c = cos(iPosRot.w);
    float s = sin(iPosRot.w);
    mat3 rotY = mat3(c, 0.0, -s,
                     0.0, 1.0, 0.0,
                     s, 0.0,  c);

    // Apply scale and height scaling; keep base anchored at ground
    vec3 p = aPos;
    // aPos is built so y in [0,1] is plant height; apply height scaling only on Y
    p.xz *= scale;
    p.y  *= hscale;

    // Simple wind sway: small lateral offset proportional to height
    // Use a tiny hash of instance pos to desync phases
    float hash = fract(dot(iPosRot.xyz, vec3(0.1031, 0.11369, 0.13787)));
    float wPhase = time * 1.7 + hash * 6.28318;
    float sway = 0.03 * sin(wPhase);
    vec3 swayOffset = vec3(sway, 0.0, 0.0) * clamp(p.y, 0.0, 1.0);

    vec3 worldP = iPosRot.xyz + rotY * (p + swayOffset);

    // Camera-relative to match terrain path
    vec3 relP = worldP - cameraPos;
    gl_Position = projection * view * model * vec4(relP, 1.0);
    // Flip V to match texture orientation (image origin at top-left)
    vUV = vec2(aUV.x, 1.0 - aUV.y);
    vType = iType;
    vHeightMask = clamp(p.y / max(0.0001, hscale), 0.0, 1.0);
    vWorldPos = worldP;
}
