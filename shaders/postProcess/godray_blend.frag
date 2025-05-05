#version 430 core

uniform sampler2D mainSceneTex;
uniform sampler2D godRayTex;

in vec2 texCoords;
out vec4 FragColor;

void main()
{
    vec3 baseColor = texture(mainSceneTex, texCoords).rgb;
    vec3 godRays = texture(godRayTex, texCoords).rgb;

    // Control how strong god rays are (tunable)
    float blendFactor = 0.2; // 0 = no rays, 1 = full intensity
    vec3 blended = mix(baseColor, baseColor + godRays, blendFactor);

    float alpha = texture(mainSceneTex, texCoords).a;
    FragColor = vec4(blended, alpha);
}
