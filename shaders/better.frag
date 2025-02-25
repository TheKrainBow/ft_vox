#version 430 core

uniform sampler2DArray textureArray;
uniform vec3 lightColor;
in vec2 TexCoord;							// The final UV coordinates calculated in the vertex shader
flat in int TextureID;

// Output color
out vec4 FragColor;       // The output color of the fragment

void main() {
	float ambientStrength = 0.3;
	vec3 ambient = ambientStrength * lightColor;

	vec4 texColor = texture(textureArray, vec3(TexCoord, TextureID));
	vec3 result = ambient * texColor.rgb;

	FragColor = vec4(result, texColor.a);
}
