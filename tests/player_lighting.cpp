// Render the real terrain fragment shader on a headless EGL context.
#define GL_GLEXT_PROTOTYPES
#include <GL/glcorearb.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>

static void require(bool condition, const char* message)
{
	if (!condition) {
		std::cerr << message << '\n';
		std::exit(1);
	}
}

static GLuint compile(GLenum type, const std::string& source)
{
	GLuint shader = glCreateShader(type);
	const char* text = source.c_str();
	glShaderSource(shader, 1, &text, nullptr);
	glCompileShader(shader);
	GLint ok;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
	if (!ok) {
		char log[4096];
		glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
		std::cerr << log << '\n';
	}
	require(ok, "Shader compilation failed");
	return shader;
}

int main()
{
	auto getDisplay = reinterpret_cast<PFNEGLGETPLATFORMDISPLAYEXTPROC>(
		eglGetProcAddress("eglGetPlatformDisplayEXT"));
	require(getDisplay != nullptr, "Surfaceless EGL unavailable");
	EGLDisplay display = getDisplay(EGL_PLATFORM_SURFACELESS_MESA, EGL_DEFAULT_DISPLAY, nullptr);
	require(eglInitialize(display, nullptr, nullptr), "EGL initialization failed");
	require(eglBindAPI(EGL_OPENGL_API), "OpenGL EGL binding failed");
	const EGLint configAttrs[] = {EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
		EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT, EGL_NONE};
	EGLConfig config;
	EGLint count;
	require(eglChooseConfig(display, configAttrs, &config, 1, &count) && count > 0,
		"EGL configuration unavailable");
	const EGLint contextAttrs[] = {EGL_CONTEXT_MAJOR_VERSION, 4,
		EGL_CONTEXT_MINOR_VERSION, 3, EGL_CONTEXT_OPENGL_PROFILE_MASK,
		EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT, EGL_NONE};
	EGLContext context = eglCreateContext(display, config, EGL_NO_CONTEXT, contextAttrs);
	require(context != EGL_NO_CONTEXT, "OpenGL 4.3 context unavailable");
	require(eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, context),
		"EGL context activation failed");

	const char* vertex = R"GLSL(#version 430 core
out vec2 TexCoord;
flat out int TextureID;
out vec3 Normal;
out vec3 FragPos;
void main() {
    vec2 p = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);
    gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
    TexCoord = vec2(0.5);
    TextureID = 0;
    Normal = vec3(0, 1, 0);
    FragPos = vec3(0);
})GLSL";
	std::ifstream file("shaders/render/terrain.frag");
	require(file.is_open(), "Run this test from the repository root");
	std::stringstream source;
	source << file.rdbuf();
	GLuint vs = compile(GL_VERTEX_SHADER, vertex);
	GLuint fs = compile(GL_FRAGMENT_SHADER, source.str());
	GLuint program = glCreateProgram();
	glAttachShader(program, vs);
	glAttachShader(program, fs);
	glLinkProgram(program);
	GLint linked;
	glGetProgramiv(program, GL_LINK_STATUS, &linked);
	require(linked, "Terrain shader link failed");
	glUseProgram(program);
	auto loc = [program](const char* name) { return glGetUniformLocation(program, name); };
	glUniform3f(loc("lightColor"), 1, 1, 1);
	glUniform3f(loc("sunDir"), 0, 1, 0);
	glUniform3f(loc("cameraPos"), 0, 1, 2);
	glUniform1i(loc("timeValue"), 64200); // noon
	glUniform1f(loc("playerLightRadius"), 10);
	glUniform1f(loc("shadowNear"), 1);
	glUniform1f(loc("shadowFar"), 500);
	// Top-down sunlight: raising the world sample moves it toward the light.
	const float identity[] = {0.02f,0,0,0, 0,0,-0.02f,0, 0,0.02f,0,0, 0,0,0,1};
	glUniformMatrix4fv(loc("lightSpaceMatrix"), 1, GL_FALSE, identity);

	GLuint atlas;
	glGenTextures(1, &atlas);
	glBindTexture(GL_TEXTURE_2D_ARRAY, atlas);
	const float texel[] = {0.2f, 0.2f, 0.2f, 1};
	glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA32F, 1, 1, 1, 0, GL_RGBA, GL_FLOAT, texel);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glUniform1i(loc("textureArray"), 0);

	GLuint shadowMap;
	glGenTextures(1, &shadowMap);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, shadowMap);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
	glUniform1i(loc("shadowMap"), 1);

	GLuint output, fbo, vao;
	glGenTextures(1, &output);
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, output);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, 1, 1, 0, GL_RGBA, GL_FLOAT, nullptr);
	glGenFramebuffers(1, &fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, output, 0);
	require(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE,
		"Test framebuffer incomplete");
	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);
	glViewport(0, 0, 1, 1);

	auto render = [&](float distance, bool enabled, bool shadowed, float shadowDepth = -1.0f) {
		glUniform3f(loc("playerLightPos"), distance, 0, 0);
		glUniform1i(loc("playerLightEnabled"), enabled);
		glActiveTexture(GL_TEXTURE1);
		const float depth = shadowDepth >= 0 ? shadowDepth : shadowed ? 0.0f : 1.0f;
		glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, 1, 1, 0,
			GL_DEPTH_COMPONENT, GL_FLOAT, &depth);
		glDrawArrays(GL_TRIANGLES, 0, 3);
		float pixel[4];
		glReadPixels(0, 0, 1, 1, GL_RGBA, GL_FLOAT, pixel);
		require(glGetError() == GL_NO_ERROR, "OpenGL render error");
		return pixel[0];
	};
	const float sun = render(0, false, false);
	const float dark = render(0, false, true);
	require(sun > dark + 0.02f, "Fixture must contain a visible sun shadow");
	// A low surface occluder blocks the sun but leaves the raised sky sample
	// exposed. A thick cave roof blocks both samples and keeps its old darkness.
	const float surfaceShade = render(0,false,true,0.40f);
	require(surfaceShade > dark+0.02f && surfaceShade < sun,
		"Surface shadows must receive gentle sky fill while caves remain dark");
	const float peak = render(0, true, true);
	require(std::abs((peak - dark) - 0.1f) < 0.0001f,
		"Peak player intensity must be half a unit of texture brightness");
	require(dark < sun * 0.1f, "Unlit caves must lose the daytime ambient fill");
	float previousBrightness = peak;
	for (float distance : {2.0f, 4.0f, 5.0f, 6.0f, 8.0f, 9.0f, 9.9f}) {
		float bright = render(distance, true, true);
		float sunny = render(distance, true, false);
		float contrast = sunny - bright;
		require(bright > dark && bright < previousBrightness,
			"Light must fade progressively throughout the entire radius");
		require(std::abs(contrast - (sun-dark)) < 0.0001f,
			"Player light must preserve natural shadow contrast");
		if (distance == 5.0f)
			require(std::abs((sunny - sun) - 0.05f) < 0.0001f,
				"Light must reach half its peak intensity halfway through the radius");
		previousBrightness = bright;
		std::cout << "Distance " << distance << ": shadowed=" << bright << ", contrast=" << contrast << '\n';
	}
	require(render(9.9f, true, true) - dark < 0.001f,
		"Light must approach the unlit value smoothly at the range boundary");
	for (float distance : {10.0f, 12.0f})
		require(std::abs(render(distance, true, true) - dark) < 0.0001f,
			"Player light must leave shadows unchanged at and beyond 10 blocks");
	glUniform1i(loc("timeValue"), 0);
	glUniform3f(loc("sunDir"),0,-1,0);
	const float night = render(12,false,false);
	glUniform1f(loc("moonLightStrength"),0.06f);
	const float moonlit = render(12,false,false);
	require(moonlit > night+0.003f && moonlit < sun*0.12f,
		"The opposite moon must illuminate terrain much less strongly than the sun");
	require(std::abs(render(12,false,true)-dark)<0.0001f,
		"Moonlight must not illuminate caves through their roofs");
	require(std::abs(render(12,false,true,0.40f)-dark)<0.0001f,
		"Surface shadow fill must disappear at night");
	require(night < sun*0.1f, "Surface terrain at night must be substantially darker than daylight");
	glUniform1i(loc("fullBright"),true);
	for (bool shadowed : {false,true})
		require(std::abs(render(12,false,shadowed)-0.2f)<0.0001f,
			"Full lighting must reveal terrain regardless of sun or occlusion");
	std::cout << "Player lighting, dark caves/night and full lighting GPU regressions passed\n";
	glDeleteVertexArrays(1, &vao);
	glDeleteFramebuffers(1, &fbo);
	glDeleteTextures(1, &output);
	glDeleteTextures(1, &shadowMap);
	glDeleteTextures(1, &atlas);
	glDeleteProgram(program);
	glDeleteShader(vs);
	glDeleteShader(fs);
	eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
	eglDestroyContext(display, context);
	eglTerminate(display);
}
