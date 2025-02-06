#version 430 core

layout(location = 0) in vec3 aPos;      // Vertex position
layout(location = 1) in vec3 worldPos;      // Vertex position

layout(location = 2) in int instanceData;   // Local position (integer)

uniform mat4 model;      // Model matrix
uniform mat4 view;       // View matrix
uniform mat4 projection; // Projection matrix

out vec2 TexCoord;
flat out int TextureID;

void main()
{
    // Add the instance position to the vertex position
	int x = (instanceData >> 0) & 0x1F;   // 5 bits for x
	int y = (instanceData >> 5) & 0x1F;   // 5 bits for y
	int z = (instanceData >> 10) & 0x1F;  // 5 bits for z
	int direction = (instanceData >> 15) & 0x07;  // 3 bits for direction
	int lengthX = (instanceData >> 18) & 0x1F;  // 5 bits for lengthX
	int lengthY = (instanceData >> 23) & 0x1F;  // 5 bits for lengthY
	int textureID = (instanceData >> 28) & 0x0F;  // 4 bits for textureID
	vec3 instancePos = vec3(float(x), float(y), float(z));
	vec3 basePos = aPos;
    vec2 finalUV = aPos.xy;
	lengthX++;
	lengthY++;
	finalUV.y = 1.0 - finalUV.y;
	basePos.x *= lengthX;
	basePos.y *= lengthY;
	if (direction == 2 || direction == 3)
		basePos.xyz = basePos.zyx;
	if (direction == 4 || direction == 5)
		basePos.zy = basePos.yz;
	if (direction == 1)
	{
		basePos.x = -basePos.x + lengthX;
		basePos.z += 1;
	}
	if (direction == 2)
	{
		basePos.y = -basePos.y + lengthY;
		finalUV.y = -finalUV.y + 1;
	}
	if (direction == 3)
		basePos.x += 1;
	if (direction == 4)
		basePos.z = -basePos.z + lengthY;
	if (direction == 5)
		basePos.y++;
    vec3 worldPosition = worldPos + basePos + instancePos;
	finalUV.x *= lengthX;
	finalUV.y *= lengthY;
    gl_Position = projection * view * model * vec4(worldPosition, 1.0);

    TexCoord = finalUV;
	TextureID = textureID;
}
