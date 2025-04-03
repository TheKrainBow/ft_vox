#version 430 core

layout (local_size_x = 16, local_size_y = 16) in;

layout (rgba32f, binding = 0) uniform image2D outputImage;

void main() {
	ivec2 pixelCoord = ivec2(gl_GlobalInvocationID.xy);

	vec4 color = imageLoad(outputImage, pixelCoord);
	imageStore(outputImage, pixelCoord, color);
}
