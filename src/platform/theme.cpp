#include <platform/tools.h>
#include <GLFW/glfw3.h>
#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#include <dwmapi.h>

// Linking with dwmapi.lib sounds like a pain, so I loaded it manually.
void customTheme(GLFWwindow* wind) {
// Define required constants if missing
#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif
#ifndef DWMWA_SYSTEMBACKDROP_TYPE
#define DWMWA_SYSTEMBACKDROP_TYPE 38
#endif

	// Define backdrop enum
	enum DWM_SYSTEMBACKDROP_TYPE {
		DWMSBT_AUTO = 0,
		DWMSBT_NONE = 1,
		DWMSBT_MAINWINDOW = 2, // Mica
		DWMSBT_TRANSIENTWINDOW = 3,
		DWMSBT_TABBEDWINDOW = 4 // Mica Alt
	};

	HWND hwnd = glfwGetWin32Window(wind);

	// Load dwmapi.dll manually
	HMODULE hDwmApi = LoadLibraryA("dwmapi.dll");
	if (!hDwmApi)
		return;
	defer(FreeLibrary(hDwmApi));

	// Define function pointer types
	using DwmSetWindowAttributeType =
		HRESULT(__stdcall*)(HWND hwnd, DWORD dwAttribute, LPCVOID pvAttribute, DWORD cbAttribute);
	using DwmExtendFrameIntoClientAreaType = HRESULT(__stdcall*)(HWND hwnd, const MARGINS* pMarInset);

	// Get functions
	auto fnDwmSetWindowAttribute = (DwmSetWindowAttributeType)GetProcAddress(hDwmApi, "DwmSetWindowAttribute");
	auto fnDwmExtendFrameIntoClientArea =
		(DwmExtendFrameIntoClientAreaType)GetProcAddress(hDwmApi, "DwmExtendFrameIntoClientArea");

	if (!fnDwmSetWindowAttribute || !fnDwmExtendFrameIntoClientArea)
		return;

	// Apply dark mode for content and titlebar (optional, works on Win10+)
	BOOL dark = TRUE;
	fnDwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));

	// Enable window shadow
	MARGINS shadow = {1, 1, 1, 1};
	fnDwmExtendFrameIntoClientArea(hwnd, &shadow);

	// Apply Mica (only if Windows 11+)
	int backdrop = DWMSBT_MAINWINDOW; // You can use DWMSBT_TABBEDWINDOW for higher contrast
	fnDwmSetWindowAttribute(hwnd, DWMWA_SYSTEMBACKDROP_TYPE, &backdrop, sizeof(backdrop));
}
#elif defined(__linux__) 
#define GLFW_EXPOSE_NATIVE_X11
#include <GLFW/glfw3native.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>

void customTheme(GLFWwindow* wind) {
	// Detect platform at runtime
	int platform = glfwGetPlatform();
	if (platform == GLFW_PLATFORM_X11) {
		// --- X11 theming ---
		Display* display = glfwGetX11Display();
		Window window = glfwGetX11Window(wind);

		// Request dark titlebar
		Atom gtk_theme_variant = XInternAtom(display, "_GTK_THEME_VARIANT", False);
		XChangeProperty(display, window, gtk_theme_variant, XA_STRING, 8, PropModeReplace, (unsigned char*)"dark", 4);

		// Set window class (helps window managers and themes)
		XClassHint* classHint = XAllocClassHint();
		if (classHint) {
			classHint->res_name = const_cast<char*>("myapp");
			classHint->res_class = const_cast<char*>("MyAppClass");
			XSetClassHint(display, window, classHint);
			XFree(classHint);
		}

		XFlush(display);
	} else if (platform == GLFW_PLATFORM_WAYLAND) {
		// --- Wayland theming ---
		// No direct control; Wayland doesn't support client-side theming
	}
}
#else
void customTheme(GLFWwindow* wind) {
}
#endif