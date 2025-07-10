#include <iostream>
#include <chrono>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glad/errorReporting.h>
#include "platform/input.h"
#include "platform/tools.h"
#include "platform/window.h"
#include "gameLogic.h"
#include <cmath>

#ifdef _WIN32
extern "C" __declspec(dllimport) int __stdcall AllocConsole();
#endif

static GLFWwindow* wind = nullptr;
static bool windowFocus = true;
static bool currentFullScreen = false;
static bool fullScreen = false;
static bool mouseMovedFlag = 0;

#pragma region callbacks

//https://stackoverflow.com/questions/21421074/how-to-create-a-full-screen-window-on-the-current-monitor-with-glfw
GLFWmonitor* getCurrentMonitor(GLFWwindow* window) {
	int nmonitors, i;
	int wx, wy, ww, wh;
	int mx, my, mw, mh;
	int overlap, bestoverlap;
	GLFWmonitor* bestmonitor;
	GLFWmonitor** monitors;
	const GLFWvidmode* mode;

	bestoverlap = 0;
	bestmonitor = NULL;

	glfwGetWindowPos(window, &wx, &wy);
	glfwGetWindowSize(window, &ww, &wh);
	monitors = glfwGetMonitors(&nmonitors);

	for (i = 0; i < nmonitors; i++) {
		mode = glfwGetVideoMode(monitors[i]);
		glfwGetMonitorPos(monitors[i], &mx, &my);
		mw = mode->width;
		mh = mode->height;

		overlap = std::max(0, std::min(wx + ww, mx + mw) - std::max(wx, mx)) *
				  std::max(0, std::min(wy + wh, my + mh) - std::max(wy, my));

		if (bestoverlap < overlap) {
			bestoverlap = overlap;
			bestmonitor = monitors[i];
		}
	}

	return bestmonitor;
}

void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {

	if ((action == GLFW_REPEAT || action == GLFW_PRESS) && key == GLFW_KEY_BACKSPACE) {
		platform::internal::addToTypedInput(8);
	}

	bool state = 0;

	if (action == GLFW_PRESS) {
		state = 1;
	} else if (action == GLFW_RELEASE) {
		state = 0;
	} else {
		return;
	}
	if (key >= GLFW_KEY_A && key <= GLFW_KEY_Z) {
		int index = key - GLFW_KEY_A;
		platform::internal::setButtonState(platform::Button::A + index, state);
	} else if (key >= GLFW_KEY_0 && key <= GLFW_KEY_9) {
		int index = key - GLFW_KEY_0;
		platform::internal::setButtonState(platform::Button::NR0 + index, state);
	} else if (key >= GLFW_KEY_F1 && key <= GLFW_KEY_F12) {
		int index = key - GLFW_KEY_F1;
		platform::internal::setButtonState(platform::Button::F1 + index, state);
	} else {
		//special keys
		//GLFW_KEY_SPACE, GLFW_KEY_ENTER, GLFW_KEY_ESCAPE, GLFW_KEY_UP, GLFW_KEY_DOWN, GLFW_KEY_LEFT, GLFW_KEY_RIGHT

		if (key == GLFW_KEY_SPACE) {
			platform::internal::setButtonState(platform::Button::Space, state);
		} else if (key == GLFW_KEY_ENTER) {
			platform::internal::setButtonState(platform::Button::Enter, state);
		} else if (key == GLFW_KEY_ESCAPE) {
			platform::internal::setButtonState(platform::Button::Escape, state);
		} else if (key == GLFW_KEY_UP) {
			platform::internal::setButtonState(platform::Button::Up, state);
		} else if (key == GLFW_KEY_DOWN) {
			platform::internal::setButtonState(platform::Button::Down, state);
		} else if (key == GLFW_KEY_LEFT) {
			platform::internal::setButtonState(platform::Button::Left, state);
		} else if (key == GLFW_KEY_RIGHT) {
			platform::internal::setButtonState(platform::Button::Right, state);
		} else if (key == GLFW_KEY_LEFT_CONTROL) {
			platform::internal::setButtonState(platform::Button::LeftCtrl, state);
		} else if (key == GLFW_KEY_TAB) {
			platform::internal::setButtonState(platform::Button::Tab, state);
		} else if (key == GLFW_KEY_LEFT_SHIFT) {
			platform::internal::setButtonState(platform::Button::LeftShift, state);
		} else if (key == GLFW_KEY_LEFT_ALT) {
			platform::internal::setButtonState(platform::Button::LeftAlt, state);
		} else if (key == GLFW_KEY_HOME) {
			platform::internal::setButtonState(platform::Button::Home, state);
		} else if (key == GLFW_KEY_END) {
			platform::internal::setButtonState(platform::Button::End, state);
		} else if (key == GLFW_KEY_DELETE) {
			platform::internal::setButtonState(platform::Button::Delete, state);
		} else if (key == GLFW_KEY_BACKSPACE) {
			platform::internal::setButtonState(platform::Button::Backspace, state);
		}
	}
};

void mouseCallback(GLFWwindow* window, int key, int action, int mods) {
	bool state = 0;

	if (action == GLFW_PRESS) {
		state = 1;
	} else if (action == GLFW_RELEASE) {
		state = 0;
	} else {
		return;
	}

	if (key == GLFW_MOUSE_BUTTON_LEFT) {
		platform::internal::setLeftMouseState(state);
	} else if (key == GLFW_MOUSE_BUTTON_RIGHT) {
		platform::internal::setRightMouseState(state);
	}
}

void windowFocusCallback(GLFWwindow* window, int focused) {
	if (focused) {
		windowFocus = 1;
	} else {
		windowFocus = 0;
		//if you not capture the release event when the window loses focus,
		//the buttons will stay pressed
		platform::internal::resetInputsToZero();
	}
}

void windowSizeCallback(GLFWwindow* window, int x, int y) {
	platform::internal::resetInputsToZero();
	glViewport(0, 0, x, y);
}

void cursorPositionCallback(GLFWwindow* window, double xpos, double ypos) {
	mouseMovedFlag = true;
}

void characterCallback(GLFWwindow* window, unsigned int codepoint) {
	platform::internal::addToTypedInput(codepoint);
}

#pragma endregion
#pragma region platform functions
namespace platform
{
void setRelMousePosition(int x, int y) {
	glfwSetCursorPos(wind, x, y);
}

bool isFullScreen() {
	return fullScreen;
}

void setFullScreen(bool f) {
	fullScreen = f;
}

void getFrameBufferSize(int* x, int* y) {
	glfwGetFramebufferSize(wind, x, y);
}

void getRelMousePosition(int* x, int* y) {
	double xI = 0, yI = 0;
	glfwGetCursorPos(wind, &xI, &yI);
	*x = floor(xI);
	*y = floor(yI);
}

void getWindowSize(int* x, int *y) {
	glfwGetWindowSize(wind, x, y);
}

void showMouse(bool show) {
	if (show) {
		glfwSetInputMode(wind, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
	} else {
		glfwSetInputMode(wind, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
	}
}

bool hasFocused() {
	return windowFocus;
}

bool mouseMoved() {
	return mouseMovedFlag;
}

}; // namespace platform

#pragma endregion

int main() {

	#ifdef _WIN32
#ifdef _MSC_VER
#if PRODUCTION_BUILD == 0
	AllocConsole();
	(void)freopen("conin$", "r", stdin);
	(void)freopen("conout$", "w", stdout);
	(void)freopen("conout$", "w", stderr);
	std::cout.sync_with_stdio();
#endif
#endif
#endif

	glfwWindowHint(GLFW_SAMPLES, 4);

#ifdef __APPLE__
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, 1);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
#endif
	int w = 800;
	int h = 600;
	permaAssert(glfwInit());
	
	wind = glfwCreateWindow(w, h, "tem", nullptr, nullptr);
	permaAssert(wind != nullptr);
	glfwMakeContextCurrent(wind);
	glfwSwapInterval(1);

	glfwSetKeyCallback(wind, keyCallback);
	glfwSetMouseButtonCallback(wind, mouseCallback);
	glfwSetWindowFocusCallback(wind, windowFocusCallback);
	glfwSetWindowSizeCallback(wind, windowSizeCallback);
	glfwSetCursorPosCallback(wind, cursorPositionCallback);
	glfwSetCharCallback(wind, characterCallback);

	permaAssert(gladLoadGLLoader((GLADloadproc)glfwGetProcAddress));
	enableReportGlErrors();

	startGame();
	auto stop = std::chrono::high_resolution_clock::now();
	while (!glfwWindowShouldClose(wind)) {
		auto start = std::chrono::high_resolution_clock::now();

		float nonAugmentedDeltaTime =
			(std::chrono::duration_cast<std::chrono::nanoseconds>(start - stop)).count() / 1000000000.0;
		stop = std::chrono::high_resolution_clock::now();

		float deltaTime = nonAugmentedDeltaTime;
		if (deltaTime > 1.f / 10) {
			deltaTime = 1.f / 10;
		}
		if (!gameLogic(deltaTime)) {
			break;
		}

		if (platform::hasFocused() && currentFullScreen != fullScreen) {
			static int lastW = w;
			static int lastH = w;
			static int lastPosX = 0;
			static int lastPosY = 0;

			if (fullScreen) {
				lastW = w;
				lastH = h;

				//glfwWindowHint(GLFW_DECORATED, NULL); // Remove the border and titlebar..
				glfwGetWindowPos(wind, &lastPosX, &lastPosY);


				//auto monitor = glfwGetPrimaryMonitor();
				auto monitor = getCurrentMonitor(wind);


				const GLFWvidmode* mode = glfwGetVideoMode(monitor);

				// switch to full screen
				glfwSetWindowMonitor(wind, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);

				currentFullScreen = 1;

			} else {
				//glfwWindowHint(GLFW_DECORATED, GLFW_TRUE); //
				glfwSetWindowMonitor(wind, nullptr, lastPosX, lastPosY, lastW, lastH, 0);

				currentFullScreen = 0;
			}
		}

		mouseMovedFlag = 0;
		platform::internal::updateAllButtons(deltaTime);
		platform::internal::resetTypedInput();

		glfwSwapBuffers(wind);
		glfwPollEvents();
	}
	closeGame();
	glfwTerminate();
	return 0;
}