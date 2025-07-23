#include "gameLogic.h"
#include <platform/window.h>
#include <platform/input.h>
#include <platform/shell.h>
#include <iostream>
#include "renderer.h"
#include "utf8.h"
#include "main.h"
#include "processOutput.h"
#include "processInput.h"
#include "styledScreen.h"
Data o;
platform::Process shell;

bool gameLogic(float deltaTime) {
	int screenW, screenH;
	platform::getWindowSize(&screenW, &screenH);
	if (platform::isButtonPressed(platform::Button::F11))
		platform::setFullScreen(!platform::isFullScreen());

	// Assumes monospace font
	o.cols = screenH / o.fontSize;
	o.rows = 2 * screenW / o.fontSize;
	o.screen.resize(o.rows, o.cols);

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
	o.fontSize = 18.0f;
	shell.launch();
	startRender();

	o.flags = TermFlags::INPUT_ECHO | TermFlags::OUTPUT_ESCAPE_CODES | TermFlags::SHOW_CURSOR | TermFlags::CURSOR_BLINK;
#ifdef _WIN32
	o.flags |= TermFlags::INPUT_LF_TO_CRLF;
#else
#endif
}
