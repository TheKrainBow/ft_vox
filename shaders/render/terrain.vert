#version 460 core
#extension GL_ARB_shader_draw_parameters : require

layout(location = 0) in ivec3 aPos;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform vec3 cameraPos; // world-space camera position

layout(std430, binding = 3) readonly buffer ssbo1 {
    vec4 ssbo[]; // per-draw subchunk origin (xyz) + res (w)
};
layout(std430, binding = 4) readonly buffer instBuf {
    int inst[];  // packed instance ints for all draws
};
layout(std430, binding = 7) readonly buffer drawMetaBuf {
    uint drawMeta[]; // per-draw flags (bit0..2 = direction)
};

out vec2 TexCoord;
flat out int TextureID;
out vec3 Normal;
out vec3 FragPos;

const float LOG_INSET = 0.10; // shrink logs/leaves a bit so they look rounded

void main() {
	// subchunk origin for this draw
	vec4 ssboValue = ssbo[gl_DrawID];
	float res = ssboValue.w;

	// instance data (baseInstance + instanceID)
	int instanceData = inst[gl_BaseInstance + gl_InstanceID];

	int x         = (instanceData >>  0) & 0x1F;
	int y         = (instanceData >>  5) & 0x1F;
	int z         = (instanceData >> 10) & 0x1F;
    // Direction now comes from per-draw meta
    int direction = int(drawMeta[gl_DrawID] & 0x7u);
    int lengthX   = (instanceData >> 15) & 0x1F;
    int lengthY   = (instanceData >> 20) & 0x1F;
    int textureID = (instanceData >> 25) & 0x7F;

	lengthX++; lengthY++; // greedy lengths are stored (len-1)

	vec3 instancePos = vec3(x, y, z);
	vec3 basePos     = vec3(aPos);
	vec2 finalUV     = vec2(aPos.x, aPos.y);

	finalUV.y = 1.0 - finalUV.y;
	finalUV /= res;

	basePos.x *= float(lengthX);
	basePos.y *= float(lengthY);

	vec3 normal = vec3(0.0);

	if (direction == 2 || direction == 3)
	{
		basePos.xyz = basePos.zyx;
	}
	if (direction == 4 || direction == 5)
	{
		basePos.zy = basePos.yz;
	}

	// Direction notes, paired with facing direction for light debug
	// Direction 0 == North
	// Direction 1 == South
	// Direction 2 == West
	// Direction 3 == East
	// Direction 4 == Down
	// Direction 5 == Up
	if (direction == 0) normal = vec3(0,0,1);
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

	if (textureID == 10 || textureID == 9) {
		float sx = max(1 - 2.0 * LOG_INSET, 0.0) / 1;
		float sz = max(1 - 2.0 * LOG_INSET, 0.0) / 1;

		float cx = 0.5 * 1;
		float cz = 0.5 * 1;

		basePos.x = cx + (basePos.x - cx) * sx;
		basePos.z = cz + (basePos.z - cz) * sz;
	}

	vec3 worldPosition = ssboValue.xyz + basePos + instancePos;
	finalUV *= vec2(float(lengthX), float(lengthY));

	// Camera-relative rendering to avoid precision cracks at large coords
	vec3 relPos = worldPosition - cameraPos;
	gl_Position = projection * view * model * vec4(relPos, 1.0);
	TexCoord = finalUV;
	TextureID = textureID;
	Normal = mat3(transpose(inverse(model))) * normal;
	FragPos = worldPosition;
}
