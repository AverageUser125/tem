#include "gameLogic.h"
#include <platform/window.h>
#include <platform/input.h>
#include <platform/shell.h>
#include <platform/tools.h>
#include <iostream>
#include "renderer.h"
#include "utf8.h"
#include "main.h"
#include "processOutput.h"
#include "processInput.h"
#include "styledScreen.h"
#include <cmath>
Data o;
static platform::Process shell;

bool gameLogic(float deltaTime) {
	int screenW, screenH;
	platform::getFrameBufferSize(&screenW, &screenH);
	if (platform::isButtonPressed(platform::Button::F11))
		platform::setFullScreen(!platform::isFullScreen());

	if (o.needResize) {
		// for changing graphics mode modes
		o.screen.resize(o.rows, o.cols);
		shell.launch(o.rows, o.cols);
		platform::setWindowSize(o.rows * o.fontWidth, o.cols * o.fontHeight);
		o.needResize = false;
	}
	if (platform::hasWindowSizeChanged()) {
		int w, h;
		platform::getWindowSize(&w, &h);
		o.cols = std::round(h / o.fontHeight);
		o.rows = std::round(w / o.fontWidth);
		o.screen.resize(o.rows, o.cols);
		shell.resize(o.rows, o.cols);
	}

	processInput();
	shell.write(o.command.data(), o.command.size());
	o.command.clear();

	shell.update();
	auto& buf = shell.getOutputBuffer();
	if (!buf.empty()) {
		processPartialOutputSegment(buf);
		buf.clear();
	}
	int scroll = platform::getScrollLevel();
	o.scrollbackOffset += scroll;
	if (o.scrollbackOffset <= 0) {
		o.scrollbackOffset = 0;
	} else if (o.scrollbackOffset >= o.screen.getScrollbackSize()) {
		o.scrollbackOffset = o.screen.getScrollbackSize() - 1;
	}
	auto lines = o.screen.getSnapshotView(o.scrollbackOffset);
	render(lines, screenW, screenH);
	if (o.flags.has(TermFlags::SHOW_CURSOR)) {
		renderCursor(o.cursorX, o.cursorY + o.scrollbackOffset, deltaTime, screenW, screenH);
	}
	return shell.isRunning();
}

void closeGame() {
	shell.terminate();
	stopRender();
}

void startGame() {
	o.cols = 25;
	o.rows = 80;
	startRender(); // loads the font, and sets o.fontWidth and o.fontHeight
	o.screen.resize(o.rows, o.cols);
	shell.launch(o.rows, o.cols);
	platform::setWindowSize(o.rows * o.fontWidth, o.cols * o.fontHeight);
	platform::changeVisibility(true);

	o.flags = TermFlags::INPUT_ECHO | TermFlags::OUTPUT_ESCAPE_CODES | TermFlags::SHOW_CURSOR | TermFlags::CURSOR_BLINK | TermFlags::OUTPUT_WRAP_LINES;
#ifdef _WIN32
	o.flags |= TermFlags::INPUT_LF_TO_CRLF;
#else
#endif
}
