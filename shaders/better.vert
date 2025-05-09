#version 460 core

layout(location = 0) in ivec3 aPos;      // Vertex position
layout(location = 2) in int instanceData; // Encoded instance data

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

layout(binding = 3, std430) readonly buffer ssbo1 {
    vec4 ssbo[];
};

out vec2 TexCoord;
flat out int TextureID;
out vec3 Normal; // Output normal for lighting
out vec3 FragPos; // Output fragment position for lighting

void main()
{
    vec4 ssboValue = ssbo[gl_DrawID];
    float res = ssboValue.w;
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
    finalUV /= res;
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
        basePos.z += res;
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
        basePos.x += res;
        normal = vec3(1.0, 0.0, 0.0);
    }
    if (direction == 4) // -Z
    {
        basePos.z = -basePos.z + lengthY;
        normal = vec3(0.0, 0.0, -1.0);
    }
    if (direction == 5) // +Y
    {
        basePos.y += res;
        normal = vec3(0.0, 1.0, 0.0);
    }

    if (textureID == 6)
    {
        basePos.y -= 0.1;
    }
    // Compute world position and transform normal to world space
    vec3 worldPosition = ssboValue.xyz + basePos + instancePos;
    finalUV.x *= lengthX;
    finalUV.y *= lengthY;

    gl_Position = projection * view * model * vec4(worldPosition, 1.0);
    TexCoord = finalUV;
    TextureID = textureID;
    Normal = mat3(transpose(inverse(model))) * normal; // Transform normal to world space
    FragPos = worldPosition; // Pass world position to fragment shader
}
