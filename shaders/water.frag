#version 430 core

in vec3 FragPos;

uniform sampler2D screenTexture;
uniform vec2 screenSize;
uniform vec3 viewPos;
uniform float waterHeight;
uniform mat4 view;
uniform mat4 projection;

out vec4 FragColor;

void main()
{
    // View direction from camera to water surface fragment
    vec3 viewDir = normalize(FragPos - viewPos);

    // Reflect view direction across water surface
    vec3 reflectedDir = reflect(viewDir, vec3(0.0, 1.0, 0.0));

    // Project reflected point to screen
    vec3 reflectedPoint = FragPos + reflectedDir * 100.0;
    vec4 clip = projection * view * vec4(reflectedPoint, 1.0);

    if (clip.w <= 0.0) {
        FragColor = vec4(0.0, 0.0, 0.0, 0.0); // fully transparent
        return;
    }

    vec3 ndc = clip.xyz / clip.w;
    vec2 uv = ndc.xy * 0.5 + 0.5;

    // UV valid?
    bool outOfBounds = any(lessThan(uv, vec2(0.0))) || any(greaterThan(uv, vec2(1.0)));

    // Sample reflection or use sky color fallback
    vec3 skyColor = vec3(0.53f, 0.81f, 0.92f); // soft sky blue
    vec3 reflectedColor = outOfBounds ? skyColor : texture(screenTexture, uv).rgb;

    // Fresnel
    float facingRatio = max(dot(viewDir, vec3(0.0, 1.0, 0.0)), 0.0);
    float fresnel = pow(1.0 - facingRatio, 3.0);

    // Camera height fade (affects reflection only)
    float cameraHeight = viewPos.y - waterHeight;
    float heightFade = clamp(1.0 - (cameraHeight / 150.0), 0.0, 1.0);

    // Blue tint stays consistent
    vec3 blueTint = vec3(0.1, 0.2, 0.5);

    // Apply fresnel to reflection only, then fade reflection with height
    vec3 reflection = mix(reflectedColor, blueTint, 0.2 * fresnel);
    vec3 finalColor = mix(blueTint, reflection, heightFade);

    // Alpha only uses fresnel now (not height)
    float alpha = mix(0.3, 0.7, fresnel);

    FragColor = vec4(finalColor, alpha);
}
