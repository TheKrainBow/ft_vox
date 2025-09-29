#version 460 core

in vec2 TexCoord;
flat in int TextureID;

uniform sampler2DArray textureArray;

// 4x4 Bayer matrix for ordered dithering (0..1)
float bayer4x4(int x, int y)
{
    const float m[16] = float[16](
        0.0/16.0,  8.0/16.0,  2.0/16.0, 10.0/16.0,
        12.0/16.0, 4.0/16.0, 14.0/16.0,  6.0/16.0,
        3.0/16.0, 11.0/16.0,  1.0/16.0,  9.0/16.0,
        15.0/16.0, 7.0/16.0, 13.0/16.0,  5.0/16.0);
    int i = (y & 3) * 4 + (x & 3);
    return m[i];
}

void main()
{
    // Alpha-tested shadow for leaves only (T_LEAF == 11)
    if (TextureID == 11) {
        float a = texture(textureArray, vec3(TexCoord, float(TextureID))).a;
        // Ordered dithering of alpha → depth coverage
        float threshold = bayer4x4(int(gl_FragCoord.x), int(gl_FragCoord.y));
        if (a < threshold) discard;
    }
    // depth-only otherwise
}
