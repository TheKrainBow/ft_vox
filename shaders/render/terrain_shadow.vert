#version 460 core
#extension GL_ARB_shader_draw_parameters : require

layout(location = 0) in ivec3 aPos;

layout(std430, binding = 3) readonly buffer ssbo1 {
    vec4 ssbo[]; // per-draw subchunk origin (xyz) + res (w)
};
layout(std430, binding = 4) readonly buffer instBuf {
    int inst[];  // packed instance ints for all draws
};
layout(std430, binding = 7) readonly buffer drawMetaBuf { uint drawMeta[]; };

uniform mat4 lightSpaceMatrix;

out vec2 TexCoord;
flat out int TextureID;

const float LOG_INSET = 0.10; // must match terrain.vert

void main() {
    // Decode draw and instance data (mirrors terrain.vert)
    vec4 ssboValue = ssbo[gl_DrawID];
    float res = ssboValue.w;

    int instanceData = inst[gl_BaseInstance + gl_InstanceID];
    int x         = (instanceData >>  0) & 0x1F;
    int y         = (instanceData >>  5) & 0x1F;
    int z         = (instanceData >> 10) & 0x1F;
    int direction = int(drawMeta[gl_DrawID] & 0x7u);
    int lengthX   = (instanceData >> 15) & 0x1F;
    int lengthY   = (instanceData >> 20) & 0x1F;
    int textureID = (instanceData >> 25) & 0x7F;
    lengthX++; lengthY++;

    vec3 instancePos = vec3(x, y, z);
    vec3 basePos     = vec3(aPos);
    vec2 finalUV     = vec2(aPos.x, aPos.y);
    finalUV.y = 1.0 - finalUV.y;
    finalUV /= res;

    basePos.x *= float(lengthX);
    basePos.y *= float(lengthY);

    if (direction == 2 || direction == 3) {
        basePos.xyz = basePos.zyx;
    }
    if (direction == 4 || direction == 5) {
        basePos.zy = basePos.yz;
    }

    // Apply per-face transforms and UV adjustments (match terrain.vert)
    if (direction == 1) {
        basePos.x = -basePos.x + lengthX;
        basePos.z += res;
    }
    if (direction == 2) {
        basePos.y = -basePos.y + lengthY;
        finalUV.y = 1.0 - finalUV.y;
    }
    if (direction == 3) {
        basePos.x += res;
    }
    if (direction == 4) {
        basePos.z = -basePos.z + lengthY;
    }
    if (direction == 5) {
        basePos.y += res;
    }

    // Inset logs/leaves a bit to match main render
    if (textureID == 10 || textureID == 9) {
        float sx = max(1 - 2.0 * LOG_INSET, 0.0) / 1;
        float sz = max(1 - 2.0 * LOG_INSET, 0.0) / 1;
        float cx = 0.5 * 1;
        float cz = 0.5 * 1;
        basePos.x = cx + (basePos.x - cx) * sx;
        basePos.z = cz + (basePos.z - cz) * sz;
    }

    vec3 worldPosition = ssboValue.xyz + basePos + instancePos;
    // Slightly shrink leaves toward their block center like alpha.vert
    if (textureID == 11) {
        const float LEAF_SCALE = 0.98;
        vec3 blockCenter = ssboValue.xyz + instancePos + vec3(0.5 * res);
        worldPosition = blockCenter + (worldPosition - blockCenter) * LEAF_SCALE;
    }
    finalUV *= vec2(float(lengthX), float(lengthY));

    gl_Position = lightSpaceMatrix * vec4(worldPosition, 1.0);
    TexCoord = finalUV;
    TextureID = textureID;
}
