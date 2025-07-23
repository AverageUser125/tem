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
	o.shell->write(o.command.data(), o.command.size());
	o.command.clear();

	o.shell->update();
	auto& buf = o.shell->getOutputBuffer();
	if (!buf.empty()) {
		processPartialOutputSegment(buf);
		buf.clear();
	}
	int scroll = platform::getScrollLevel();
	o.scrollbackOffset += scroll;
	if (o.scrollbackOffset <= 0) {
		o.scrollbackOffset = 0;
	} else if (o.scrollbackOffset >= o.screen.get_scrollback_size()) {
		o.scrollbackOffset = o.screen.get_scrollback_size() - 1;
	}
	auto lines = o.screen.get_snapshot_view(o.scrollbackOffset);
	render(lines, screenW, screenH);
	if (o.flags.has(TermFlags::SHOW_CURSOR)) {
		renderCursor(o.cursorX, o.cursorY + o.scrollbackOffset, deltaTime, screenW, screenH);
	}
	return o.shell->isRunning();
}

void closeGame() {
	delete o.shell;
	stopRender();
}

void startGame() {
	o.shell = platform::launch();
	startRender(o.fontSize);

	o.flags = TermFlags::INPUT_ECHO | TermFlags::OUTPUT_ESCAPE_CODES | TermFlags::SHOW_CURSOR;
#ifdef _WIN32
	o.flags |= TermFlags::INPUT_LF_TO_CRLF;
#else
#endif
}
