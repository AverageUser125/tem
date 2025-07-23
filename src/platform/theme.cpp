#ifdef _WIN32
#include <platform/tools.h>
#include <GLFW/glfw3.h>
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

#else
void customTheme(GLFWwindow* wind) {
}
#endif