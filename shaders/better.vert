//#version 330 core
#version 460 core
#extension GL_ARB_shader_draw_parameters : enable

layout(std430, binding = 0) buffer ChunkPositions {
    vec4 chunkPositions[];  // Array of chunk positions
};

layout(location = 0) in vec3 aPos;      // Vertex position
//layout(location = 1) in vec3 worldPos;      // Vertex position

layout(location = 1) in int instanceData;   // Local position (integer)

uniform mat4 model;      // Model matrix
uniform mat4 view;       // View matrix
uniform mat4 projection; // Projection matrix

out vec2 TexCoord;

void main()
{
    // Add the instance position to the vertex position
    int x = (instanceData >> 0) & 0x1F;   // 5 bits for x (0-63)
    int y = (instanceData >> 5) & 0x1F;   // 5 bits for y (0-63)
    int z = (instanceData >> 10) & 0x1F;  // 5 bits for z (0-63)
    int direction = (instanceData >> 15) & 0x07;  // 3 bits for direction
    int textureID = (instanceData >> 18) & 0x7F;  // 7 bits for textureID
	vec3 instancePos = vec3(float(x), float(y), float(z));
	vec3 basePos = aPos;
    vec2 finalUV = aPos.xy;
	if (direction == 2 || direction == 3)
		basePos.xyz = basePos.zyx;
	if (direction == 4 || direction == 5)
		basePos.zy = basePos.yz;
	if (direction == 1)
	{
		basePos.x = -basePos.x + 1;
		basePos.z++;
	}
	if (direction == 2)
	{
		basePos.y = -basePos.y + 1;
		finalUV.y = -finalUV.y + 1;
	}
	if (direction == 3)
		basePos.x++;
	if (direction == 4)
		basePos.z = -basePos.z + 1;
	if (direction == 5)
		basePos.y++;
	vec3 chunkPos = chunkPositions[gl_BaseInstance + gl_InstanceID].xyz;
    vec3 worldPosition = chunkPos + basePos + instancePos;
    //vec3 worldPosition = basePos + instancePos;
    //vec3 worldPosition = basePos;
	//worldPosition.x += gl_InstanceID;
	//vec3 worldPosition = basePos + vec3(gl_InstanceID, 0.0, 0.0);
    //vec3 worldPosition = basePos + instancePos;
	//worldPosition.x += chunkPositions[gl_InstanceID].x;
    finalUV.y = finalUV.y / 5 + (float(textureID) / 5);
    gl_Position = projection * view * model * vec4(worldPosition, 1.0);
    TexCoord = finalUV;
}
