#version 460 core

#extension GL_ARB_shader_draw_parameters : require
layout(location = 0) in ivec3 aPos;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform vec3 cameraPos; // world-space camera position
uniform float time;

layout(std430, binding = 3) readonly buffer ssbo1 { vec4 ssbo[]; };
layout(std430, binding = 4) readonly buffer instBuf { int inst[]; };
layout(std430, binding = 7) readonly buffer drawMetaBuf { uint drawMeta[]; };

out vec2 TexCoord;
flat out int TextureID;
out vec3 Normal;
out vec3 FragPos;

void main()
{
	vec4 ssboValue = ssbo[gl_DrawID];
	float res = ssboValue.w;

	int instanceData = inst[gl_BaseInstance + gl_InstanceID];

    int x = (instanceData >>  0) & 0x1F;
    int y = (instanceData >>  5) & 0x1F;
    int z = (instanceData >> 10) & 0x1F;
    int direction = int(drawMeta[gl_DrawID] & 0x7u);
    int lengthX   = (instanceData >> 15) & 0x1F;
    int lengthY   = (instanceData >> 20) & 0x1F;
    int textureID = (instanceData >> 25) & 0x7F;

    // Keep only WATER in this pass
    if (textureID != 6) {
        gl_Position = vec4(2.0, 2.0, 2.0, 1.0); // outside clip
        TexCoord = vec2(0.0); TextureID = textureID; Normal = vec3(0.0); FragPos = vec3(0.0);
        return;
    }

	vec3 instancePos = vec3(x, y, z);
	vec3 basePos = aPos;
	vec2 finalUV = aPos.xy;

	lengthX++; lengthY++;
	finalUV.y = 1.0 - finalUV.y;
	finalUV /= res;
	basePos.x *= lengthX;
	basePos.y *= lengthY;

	vec3 normal = vec3(0.0);

	if (direction == 2 || direction == 3) basePos.xyz = basePos.zyx;
	if (direction == 4 || direction == 5) basePos.zy  = basePos.yz;

    if (direction == 0) normal = vec3(0,0,1);
    if (direction == 1) { basePos.x = -basePos.x + lengthX; basePos.z += res; normal = vec3(0,-1,0); }
    if (direction == 2) { basePos.y = -basePos.y + lengthY; finalUV.y = 1.0 - finalUV.y; normal = vec3(1,0,0); }
    if (direction == 3) { basePos.x += res; normal = vec3(-1,0,0); }
    if (direction == 4) { basePos.z = -basePos.z + lengthY; normal = vec3(0,0,-1); }
    if (direction == 5) { basePos.y += res; normal = vec3(0,1,0); }

	basePos.y -= 0.1;

	vec3 worldPosition = ssboValue.xyz + basePos + instancePos;
	finalUV *= vec2(lengthX, lengthY);

	// Camera-relative to avoid precision seams
	vec3 relPos = worldPosition - cameraPos;
	gl_Position = projection * view * model * vec4(relPos, 1.0);
	TexCoord = finalUV;
	TextureID = textureID;
	Normal = mat3(transpose(inverse(model))) * normal;
	FragPos = worldPosition;
}
