#version 460 core

#extension GL_ARB_shader_draw_parameters : require
layout(location = 0) in ivec3 aPos;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

layout(std430, binding = 3) readonly buffer ssbo1 {
	vec4 ssbo[];				 // per-draw subchunk origin (xyz) + res (w)
};

layout(std430, binding = 4) readonly buffer instBuf {
	int inst[];				  // packed instance ints for all draws
};

out vec2 TexCoord;
flat out int TextureID;
out vec3 Normal;
out vec3 FragPos;

void main()
{
	// Per-draw subchunk origin: index by draw id
	vec4 ssboValue = ssbo[gl_DrawID];
	float res = ssboValue.w;

	// Instance data comes from SSBO using baseInstance + instanceID
	int instanceData = inst[gl_BaseInstance + gl_InstanceID];

	// Decode instance data
	int x = (instanceData >>  0) & 0x1F;
	int y = (instanceData >>  5) & 0x1F;
	int z = (instanceData >> 10) & 0x1F;
	int direction = (instanceData >> 15) & 0x07;
	int lengthX   = (instanceData >> 18) & 0x1F;
	int lengthY   = (instanceData >> 23) & 0x1F;
	int textureID = (instanceData >> 28) & 0x0F;

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

	// Direction notes, paired with facing direction for light debug
	// Direction 0 == North
	// Direction 1 == South
	// Direction 2 == West
	// Direction 3 == East
	// Direction 4 == Down
	// Direction 5 == Up
	if (direction == 0) normal = vec3(0,0, 1);
	if (direction == 1)
	{
		basePos.x = -basePos.x + lengthX;
		basePos.z += res;
		normal = vec3(0,-1,0);
	}
	if (direction == 2)
	{
		basePos.y = -basePos.y + lengthY;
		finalUV.y = 1.0 - finalUV.y;
		normal = vec3(1,0,0);
	}
	if (direction == 3)
	{
		basePos.x += res;
		normal = vec3(-1,0,0); }
	if (direction == 4)
	{
		basePos.z = -basePos.z + lengthY;
		normal = vec3(0,0,-1);
	}
	if (direction == 5)
	{
		basePos.y += res;
		normal = vec3(0,1,0);
	}

	vec3 worldPosition = ssboValue.xyz + basePos + instancePos;
	finalUV *= vec2(lengthX, lengthY);

	gl_Position = projection * view * model * vec4(worldPosition, 1.0);
	TexCoord = finalUV;
	TextureID = textureID;
	Normal = mat3(transpose(inverse(model))) * normal;
	FragPos = worldPosition;
}
