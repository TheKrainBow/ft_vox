#version 430 core

in vec3 FragPos;

uniform sampler2D screenTexture;
uniform sampler2D normalMap;
uniform vec2 screenSize;
uniform vec3 viewPos;
uniform float waterHeight;
uniform float time;
uniform mat4 view;
uniform mat4 projection;
uniform int isUnderwater;

out vec4 FragColor;

void main()
{
    // Match vertex wave for vertical displacement
    float waveFreq  = 0.001;
    float waveAmp   = 0.03;
    float waveSpeed = 0.01;

    float wave = sin(FragPos.x * waveFreq + time * waveSpeed)
               + cos(FragPos.z * waveFreq + time * waveSpeed * 0.8);
    wave *= 0.5 * waveAmp;

    vec3 waveFragPos = FragPos;
    waveFragPos.y += wave;

    // View direction
    vec3 viewDir = normalize(waveFragPos - viewPos);

    // Ripple: normal map UV + scroll
    vec2 rippleUV = FragPos.xz * 0.05 + vec2(time * 0.0002, time * 0.00015);
    vec3 sampledNormal = texture(normalMap, rippleUV).rgb;
    vec3 normal = normalize(sampledNormal * 2.0 - 1.0);

    // Blend with flat up vector to soften effect
    float rippleStrength = 0.05;
    normal = normalize(mix(vec3(0.0, 1.0, 0.0), normal, rippleStrength));

    vec3 reflectedDir = reflect(viewDir, normal);
    vec3 reflectedPoint = waveFragPos + reflectedDir * 100.0;
    vec4 clip = projection * view * vec4(reflectedPoint, 1.0);
    // Multiply reflection by fade factor
    

    vec3 skyColor = vec3(0.53f, 0.81f, 0.92f);
    // if (clip.w <= 0.0) {
    //     FragColor = vec4(0.1, 0.2, 0.5, 1.0);
    //     return;
    // }

    // Height fade
    float cameraHeight = viewPos.y - waveFragPos.y;
    float heightFade = clamp(1.0 - (cameraHeight / 25.0), 0.0, 0.8);
    float normalOffset = (sampledNormal.b - 0.5) * 0.2;

    if (clip.w <= 0.0) {
        vec3 blueTint = vec3(0.1, 0.2, 0.5);
        vec3 finalColor = mix(blueTint, mix(skyColor, blueTint, 0.2), heightFade);
        finalColor += normalOffset;
        // finalColor.b = clamp(finalColor.b, 0.0, 1.0);
        FragColor = vec4(finalColor, 0.5f);
        return;
    }
    vec3 ndc = clip.xyz / clip.w;
    vec2 uv = ndc.xy * 0.5 + 0.5;

    // Reflection distortion (sync with wave)
    // float waveX = sin(FragPos.x * waveFreq + time * waveSpeed);
    // float waveY = cos(FragPos.z * waveFreq + time * waveSpeed * 0.8);
    // uv += vec2(waveX, waveY) * 0.005;

    // Sample reflection or fallback to sky
    // vec2 safeUV = clamp(uv, vec2(0.001), vec2(0.999));
    // vec3 reflectedColor = texture(screenTexture, safeUV).rgb;
    bool outOfBounds = any(lessThan(uv, vec2(0.0))) || any(greaterThan(uv, vec2(1.0)));
    // vec3 skyColor = vec3(1.0f, 0.0f, 0.0f);
    vec3 reflectedColor = outOfBounds ? skyColor : texture(screenTexture, uv).rgb;


    // Final color blending
    vec3 blueTint = vec3(0.1, 0.2, 0.5);
    vec3 finalColor;

    if (isUnderwater == 1) {
        finalColor = vec3(0.0, 0.1, 0.3); // no reflection underwater
    } else {
        vec3 reflection = mix(reflectedColor, blueTint, 0.2);
        finalColor = mix(blueTint, reflection, heightFade);
        finalColor += normalOffset;
        finalColor = clamp(finalColor, 0.0, 1.0);
    }
    FragColor = vec4(finalColor, 0.5);
}
