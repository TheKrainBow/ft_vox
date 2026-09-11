#include "MouseCapture.hpp"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <iostream>

#if defined(__linux__)
#define GLFW_EXPOSE_NATIVE_X11
#include <GLFW/glfw3native.h>
#endif

void MouseCapture::release(GLFWwindow* window)
{
	// Let GLFW release its pointer grab and raw motion before removing confinement.
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
#if defined(__linux__)
	if (_confineWindow)
	{
		Display* display = glfwGetX11Display();
		XDestroyWindow(display, _confineWindow);
		XFlush(display);
		_confineWindow = 0;
	}
#endif
}

void MouseCapture::apply(GLFWwindow* window, bool enabled)
{
	if (!enabled || !glfwGetWindowAttrib(window, GLFW_FOCUSED)
		|| glfwGetWindowAttrib(window, GLFW_ICONIFIED))
	{
		release(window);
		return;
	}
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
#if defined(__linux__)
	// If GLFW has no X11 display, keep its native pointer constraints.
	Display* display = glfwGetX11Display();
	if (!display)
		return;
	int width, height;
	glfwGetWindowSize(window, &width, &height);
	if (width < 3 || height < 3)
		return;
	const Window parent = glfwGetX11Window(window);
	const int insetX = std::min(8, (width - 1) / 2);
	const int insetY = std::min(8, (height - 1) / 2);
	if (!_confineWindow)
	{
		// InputOnly: no pixels or GL surface. Events still go to the GLFW parent.
		_confineWindow = XCreateWindow(display, parent, insetX, insetY,
			width - 2 * insetX, height - 2 * insetY, 0, 0, InputOnly,
			CopyFromParent, 0, nullptr);
		XMapWindow(display, _confineWindow);
	}
	else
		XMoveResizeWindow(display, _confineWindow, insetX, insetY,
			width - 2 * insetX, height - 2 * insetY);

	// Replace GLFW's same-client grab, narrowing its confinement rectangle.
	// Both modes stay asynchronous, so keyboard shortcuts remain available.
	// Preserve owner_events so XI2 raw events selected on the root reach GLFW.
	const int result = XGrabPointer(display, parent, True,
		ButtonPressMask | ButtonReleaseMask | PointerMotionMask,
		GrabModeAsync, GrabModeAsync, _confineWindow, None, CurrentTime);
	if (result != GrabSuccess)
		std::cerr << "Mouse confinement failed (X11 grab status " << result << ")\n";
	XFlush(display);
#endif
}
