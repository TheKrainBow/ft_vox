#pragma once

struct GLFWwindow;

// GLFW handles relative motion; the X11 confinement also excludes desktop edges.
class MouseCapture {
public:
	void apply(GLFWwindow* window, bool enabled);
	void release(GLFWwindow* window);
private:
	unsigned long _confineWindow = 0;
};
