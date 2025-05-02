#version 460 core

in vec3 FragPos;

uniform sampler2D screenTexture;
uniform sampler2D depthTexture;
uniform vec2 screenSize;

uniform mat4 projection;
uniform mat4 view;
uniform mat4 inverseProjection;
uniform vec3 viewPos;

out vec4 FragColor;

// Reconstruct view-space position from UV + depth
vec3 reconstructViewPos(vec2 uv, float depth)
{
    vec4 ndc = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 viewPos = inverseProjection * ndc;
    return viewPos.xyz / viewPos.w;
}

void main()
{
    vec2 uv = gl_FragCoord.xy / screenSize;

    // Reconstruct view-space position of this fragment (water surface)
    float depth = texture(depthTexture, uv).r;
    vec3 viewSpacePos = reconstructViewPos(uv, depth);

    // Compute view direction from eye (0,0,0 in view space) to fragment
    vec3 viewDir = normalize(viewSpacePos);

    // Reflect that direction across flat water normal
    vec3 reflectedDir = reflect(viewDir, vec3(0.0, 1.0, 0.0));

    // Start raymarching from view-space position of fragment
    vec3 rayOrigin = viewSpacePos;

    vec3 hitColor = vec3(0.0);
    bool hit = false;

    const int maxSteps = 128;
    const float stepSize = 0.2;

    for (int i = 0; i < maxSteps; ++i)
    {
        rayOrigin += reflectedDir * stepSize;

        // Project to clip space and then to screen space
        vec4 clip = projection * vec4(rayOrigin, 1.0);
        if (clip.w <= 0.0) break;

        vec3 ndc = clip.xyz / clip.w;
        vec2 screenUV = ndc.xy * 0.5 + 0.5;

        // Clamp to screen
        if (any(lessThan(screenUV, vec2(0.0))) || any(greaterThan(screenUV, vec2(1.0))))
            break;

        float sceneDepth = texture(depthTexture, screenUV).r;
        vec3 sceneViewPos = reconstructViewPos(screenUV, sceneDepth);

        float depthDiff = abs(sceneViewPos.z - rayOrigin.z);
        if (depthDiff < 0.2) {
            hitColor = texture(screenTexture, screenUV).rgb;
            hit = true;
            break;
        }
    }

    FragColor = vec4(hit ? hitColor : vec3(0.0), 1.0);
}
