#version 430 core

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoord;

uniform vec3 viewPos;
uniform float time;

uniform sampler2D screenTexture;
uniform vec2 screenSize;
uniform float waterHeight;
uniform mat4 view;
uniform mat4 projection;

out vec4 FragColor;

// Hash function for variation
float hash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123);
}

void main()
{
    // Base water colors
    vec3 shallowColor = vec3(0.0, 0.3, 0.6);
    vec3 deepColor    = vec3(0.0, 0.1, 0.3);

    // Animated wave normal
    vec2 pos = FragPos.xz * 0.025;
    float t = time * 0.25;
    float w1 = sin(pos.x + t);
    float w2 = cos(pos.y + t + hash(pos * 0.4) * 2.0);
    float w3 = sin((pos.x + pos.y) * 0.5 + t * 1.5 + hash(pos * 0.7) * 4.0);

    vec3 waveNormal = normalize(vec3(
        (w1 + w2 + w3) * 0.05,
        1.0,
        (w1 - w2 + w3 * 0.5) * 0.05
    ));

    // View direction
    vec3 viewDir = normalize(viewPos - FragPos);
    float fresnel = pow(1.0 - max(dot(viewDir, waveNormal), 0.0), 3.0);
    vec3 baseColor = mix(deepColor, shallowColor, fresnel);

    // Reflect world position across water surface
    vec3 reflectedPos = FragPos;
    reflectedPos.y = waterHeight - (FragPos.y - waterHeight);

    // Project reflected position to screen UVs
    vec4 clipSpace = projection * view * vec4(reflectedPos, 1.0);
    vec3 ndc = clipSpace.xyz / clipSpace.w;
    vec2 screenUV = ndc.xy * 0.5 + 0.5;

    // Reflection sampling — only if valid
    vec3 reflectedScene = vec3(0.0);
    if (clipSpace.w > 0.0) {
        vec3 ndc = clipSpace.xyz / clipSpace.w;
        vec2 screenUV = ndc.xy * 0.5 + 0.5;

        bool validUV = ndc.z > 0.0 &&
                    all(greaterThanEqual(screenUV, vec2(0.0))) &&
                    all(lessThanEqual(screenUV, vec2(1.0)));

        if (validUV) {
            reflectedScene = texture(screenTexture, screenUV).rgb;
        }
    }

    // Distance and fade control
    float dist = length(viewPos - FragPos);
    float distFade = 1.0 - smoothstep(100.0, 400.0, dist);
    float reflectionBoost = 1.3; // ↑ makes water more mirror-like
    float reflectionFactor = clamp(fresnel * distFade * reflectionBoost, 0.0, 1.0);

    // Blend base color with reflection
    vec3 waterColor = mix(baseColor, reflectedScene, reflectionFactor);

    // Specular (sun from above)
    vec3 lightDir = normalize(vec3(0.0, 1.0, 0.0));
    vec3 reflectDir = reflect(-lightDir, waveNormal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);
    vec3 specular = vec3(1.0) * spec * 0.25;

    // Fog
    float fogStart = 1000.0;
    float fogEnd = 3000.0;
    float fogFactor = clamp((dist - fogStart) / (fogEnd - fogStart), 0.0, 1.0);
    vec3 fogColor = vec3(0.5);

    // Final color with fog and specular
    vec3 finalColor = mix(waterColor + specular, fogColor, fogFactor);
    float alpha = mix(0.6, 0.95, fresnel);

    FragColor = vec4(finalColor, alpha);
}
