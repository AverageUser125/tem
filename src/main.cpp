#include "gameLogic.h"
#include <glad/glad.h>
#include <platform/window.h>
#include <platform/input.h>
#include <platform/shell.h>
#include <iostream>
#include "renderer.h"
#include "utf8.h"

struct ivec2 {
	int x;
	int y;
};

static platform::Process* shell = nullptr;
static std::vector<std::string> lines;
static int scrollLineStart = 0;
static float fontSize = 16.0f;
static ivec2 cursor;

static void appendNewLines(const std::vector<char>& buf) {
	static size_t processedBytes = 0;
	const char* data = buf.data();
	const char* end = data + buf.size();
	const char* p = data + processedBytes;
	const char* lineStart = p;

	while (p < end) {
		uint32_t cp;
		int len = decode_utf8(p, &cp);
		if (len <= 0 || p + len > end)
			break;

		if (cp == '\n') {
			lines.emplace_back(lineStart, p - lineStart);
			p += len;
			lineStart = p;
		} else {
			p += len;
		}
	}

	if (lineStart < p) {
		std::string partial(lineStart, p - lineStart);

		if (!lines.empty() && !lines.back().empty() && lines.back().back() != '\n') {
			lines.back() += partial;
		} else {
			lines.emplace_back(std::move(partial));
		}
	}

	processedBytes = size_t(p - data);
}


bool gameLogic() {
	int screenW, screenH;
	platform::getWindowSize(&screenW, &screenH);
	glViewport(0, 0, screenW, screenH);
	glClear(GL_COLOR_BUFFER_BIT);

	if (platform::isButtonPressed(platform::Button::F11)) {
		platform::setFullScreen(!platform::isFullScreen());
	}

	auto& buf = shell->getOutputBuffer();
	const std::string& input = platform::getTypedInput();
	if (!input.empty()) {
		shell->write(input.c_str(), input.size());
		// TODO: actuall input handeling
		buf.insert(buf.end(), input.begin(), input.end());
	}
	if (platform::isButtonPressed(platform::Button::Enter)) {
		shell->write("\r\n", 2);
	}

	shell->update();
	appendNewLines(buf);
	int totalLines = int(lines.size());
	int visibleLines = screenH / fontSize;
	if (totalLines > visibleLines) {
		scrollLineStart = totalLines - visibleLines;
	}

	render(lines, scrollLineStart, fontSize, screenW, screenH);
	return true;
}

void closeGame() {
	delete shell;
}

void startGame() {
	shell = platform::launch("cmd.exe /Q /K");
	startRender(fontSize);
}
