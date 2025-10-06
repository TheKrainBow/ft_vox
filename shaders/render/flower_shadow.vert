#version 460 core

layout(location = 0) in vec3 aPos;   // base mesh position (unit)
layout(location = 1) in vec2 aUV;    // base mesh UV (0..1)

// Instance attributes (same as flower.vert)
layout(location = 2) in vec4 iPosRot;       // xyz world pos, w = rot radians
layout(location = 3) in vec2 iScaleHeights; // x = scale jitter, y = heightScale
layout(location = 4) in int  iType;         // texture layer

uniform mat4 lightSpaceMatrix;  // cascade light view-proj

out vec2 vUV;
flat out int vType;

void main()
{
	float scale = iScaleHeights.x;
	float hscale = iScaleHeights.y;

	float c = cos(iPosRot.w);
	float s = sin(iPosRot.w);
	mat3 rotY = mat3(c, 0.0, -s,
					 0.0, 1.0, 0.0,
					 s, 0.0,  c);

	vec3 p = aPos;
	p.xz *= scale;
	p.y  *= hscale;

	vec3 worldP = iPosRot.xyz + rotY * p;
	gl_Position = lightSpaceMatrix * vec4(worldP, 1.0);
	vUV = vec2(aUV.x, 1.0 - aUV.y);
	vType = iType;
}

