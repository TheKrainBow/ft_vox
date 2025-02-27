#version 430 core

layout(location = 0) in vec3 aPos;      // Vertex position
layout(location = 1) in vec3 worldPos;  // World position
layout(location = 2) in int instanceData; // Encoded instance data

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

out vec2 TexCoord;
flat out int TextureID;
out vec3 Normal; // Output normal for lighting

void main()
{
    // Decode instance data
    int x = (instanceData >> 0) & 0x1F;
    int y = (instanceData >> 5) & 0x1F;
    int z = (instanceData >> 10) & 0x1F;
    int direction = (instanceData >> 15) & 0x07;
    int lengthX = (instanceData >> 18) & 0x1F;
    int lengthY = (instanceData >> 23) & 0x1F;
    int textureID = (instanceData >> 28) & 0x0F;

    vec3 instancePos = vec3(float(x), float(y), float(z));
    vec3 basePos = aPos;
    vec2 finalUV = aPos.xy;

    lengthX++;
    lengthY++;
    finalUV.y = 1.0 - finalUV.y;
    basePos.x *= lengthX;
    basePos.y *= lengthY;

    // Default normal
    vec3 normal = vec3(0.0, 0.0, 0.0);

    if (direction == 2 || direction == 3)
    {
        basePos.xyz = basePos.zyx;
    }
    if (direction == 4 || direction == 5)
    {
        basePos.zy = basePos.yz;
    }

	if (direction == 0)
	{
        normal = vec3(0.0, 0.0, 1.0);
	}
    if (direction == 1) // -X
    {
        basePos.x = -basePos.x + lengthX;
        basePos.z += 1;
        normal = vec3(-1.0, 0.0, 0.0);
    }
    if (direction == 2) // -Y
    {
        basePos.y = -basePos.y + lengthY;
        finalUV.y = -finalUV.y + 1;
        normal = vec3(0.0, -1.0, 0.0);
    }
    if (direction == 3) // +X
    {
        basePos.x += 1;
        normal = vec3(1.0, 0.0, 0.0);
    }
    if (direction == 4) // -Z
    {
        basePos.z = -basePos.z + lengthY;
        normal = vec3(0.0, 0.0, -1.0);
    }
    if (direction == 5) // +Y
    {
        basePos.y++;
        normal = vec3(0.0, 1.0, 0.0);
    }

    vec3 worldPosition = worldPos + basePos + instancePos;
    finalUV.x *= lengthX;
    finalUV.y *= lengthY;
    
    gl_Position = projection * view * model * vec4(worldPosition, 1.0);

    TexCoord = finalUV;
    TextureID = textureID;
    Normal = mat3(transpose(inverse(model))) * normal; // Transform normal to world space
}
