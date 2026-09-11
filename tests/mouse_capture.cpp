// Run on an isolated X11 display: this test deliberately warps the pointer.
#include "MouseCapture.hpp"
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_X11
#include <GLFW/glfw3native.h>
#include <X11/extensions/XTest.h>
#include <cstdlib>
#include <iostream>

static void require(bool condition, const char* message)
{
	if (!condition)
	{
		std::cerr << message << '\n';
		std::exit(1);
	}
}

static void warpAndCheck(Display* display, int x, int y, bool confined,
	int width = 640, int height = 480)
{
	Window root = DefaultRootWindow(display), child, returnedRoot;
	int rootX, rootY, localX, localY;
	unsigned int mask;
	XWarpPointer(display, None, root, 0, 0, 0, 0, x, y);
	XSync(display, False);
	XQueryPointer(display, root, &returnedRoot, &child, &rootX, &rootY,
		&localX, &localY, &mask);
	if (confined)
		require(rootX >= 8 && rootY >= 8 && rootX < width - 8 && rootY < height - 8,
			"Captured pointer reached the edge");
	else
		require(rootX == x && rootY == y, "Pointer remained confined after release");
}

int main()
{
	require(glfwInit(), "GLFW initialization failed");
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
	GLFWwindow* window = glfwCreateWindow(640, 480, "Mouse capture regression", nullptr, nullptr);
	require(window != nullptr, "Window creation failed");
	glfwSetWindowPos(window, 0, 0);
	glfwFocusWindow(window);
	glfwPollEvents();
	require(glfwGetWindowAttrib(window, GLFW_FOCUSED), "Test window is not focused");
	MouseCapture capture;
	glfwSetWindowUserPointer(window, &capture);
	glfwSetWindowFocusCallback(window, [](GLFWwindow* w, int focused) {
		static_cast<MouseCapture*>(glfwGetWindowUserPointer(w))->apply(w, focused);
	});
	Display* display = glfwGetX11Display();
	int mouseEvents = 0, buttonEvents = 0;
	static int* mouseCounter = &mouseEvents;
	static int* buttonCounter = &buttonEvents;
	glfwSetCursorPosCallback(window, [](GLFWwindow*, double, double) { ++*mouseCounter; });
	glfwSetMouseButtonCallback(window, [](GLFWwindow*, int, int, int) { ++*buttonCounter; });
	require(glfwRawMouseMotionSupported(), "Raw motion unavailable on test display");
	glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
	capture.apply(window, true);
	XSync(display, False);
	glfwPollEvents();
	mouseEvents = 0;
	buttonEvents = 0;
	XTestFakeRelativeMotionEvent(display, 20, 20, CurrentTime);
	XTestFakeButtonEvent(display, 1, True, CurrentTime);
	XTestFakeButtonEvent(display, 1, False, CurrentTime);
	XSync(display, False);
	glfwPollEvents();
	require(mouseEvents > 0, "Captured raw motion did not reach GLFW");
	require(buttonEvents == 2, "Captured clicks did not reach GLFW");
	// No event polling between warps: confinement must survive a stalled frame.
	warpAndCheck(display, 0, 0, true);
	warpAndCheck(display, 639, 479, true);
	warpAndCheck(display, 0, 479, true);
	warpAndCheck(display, 639, 0, true);
	capture.release(window);
	warpAndCheck(display, 0, 0, false);
	warpAndCheck(display, 639, 479, false);
	capture.apply(window, true);
	glfwSetWindowSize(window, 320, 240);
	capture.apply(window, true);
	warpAndCheck(display, 639, 479, true, 320, 240);
	glfwSetWindowMonitor(window, glfwGetPrimaryMonitor(), 0, 0, 800, 600, GLFW_DONT_CARE);
	glfwFocusWindow(window);
	glfwPollEvents();
	capture.apply(window, true);
	warpAndCheck(display, 799, 599, true, 800, 600);
	glfwSetWindowMonitor(window, nullptr, 0, 0, 320, 240, GLFW_DONT_CARE);
	glfwFocusWindow(window);
	glfwPollEvents();
	capture.apply(window, true);
	warpAndCheck(display, 799, 599, true, 320, 240);
	XSetInputFocus(display, DefaultRootWindow(display), RevertToParent, CurrentTime);
	XSync(display, False);
	glfwPollEvents();
	require(!glfwGetWindowAttrib(window, GLFW_FOCUSED), "Focus loss was not delivered");
	warpAndCheck(display, 0, 0, false);
	glfwFocusWindow(window);
	XSync(display, False);
	glfwPollEvents();
	warpAndCheck(display, 639, 479, true, 320, 240);
	capture.release(window);
	glfwDestroyWindow(window);
	warpAndCheck(display, 0, 0, false);
	glfwTerminate();
	std::cout << "Mouse confinement, raw motion, clicks, fullscreen, resize, focus loss/regain and release passed\n";
}
