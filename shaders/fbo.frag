#version 430 core

out vec4 FragColor;
in vec2 texCoords;

uniform sampler2D screenTexture;

const float offset_x = 2.0 / 1000.0;
const float offset_y = 2.0 / 800.0;


// Define the target "sky color"
const vec3 skyColor = vec3(0.53, 0.81, 0.92); // Change based on your sky
const float threshold = 0.1; // Tolerance for "how close" to sky color

void main()
{
    vec3 currentColor = texture(screenTexture, texCoords).rgb;
    
    // Check if the pixel is sky based on color similarity
    float diff = distance(currentColor, skyColor);
    bool isSky = diff < threshold;

    if (isSky)
	{
        // Sample 4 neighboring pixels and average them
        vec3 up    = texture(screenTexture, texCoords + vec2(0.0,  offset_y)).rgb;
        vec3 down  = texture(screenTexture, texCoords + vec2(0.0, -offset_y)).rgb;
        vec3 left  = texture(screenTexture, texCoords + vec2(-offset_x, 0.0)).rgb;
        vec3 right = texture(screenTexture, texCoords + vec2( offset_x, 0.0)).rgb;

        vec3 blended = (up + down + left + right) / 4.0;
        FragColor = vec4(blended, 1.0);
    }
	else
	{
        FragColor = vec4(currentColor, 1.0);
    }
}
