#include "gameLogic.h"
#include <platform/window.h>
#include <platform/input.h>
#include <platform/shell.h>
#include <iostream>
#include "renderer.h"
#include "utf8.h"
#include "main.h"
#include "processText.h"
#include "styledScreen.h"
Data o;

static void incCurX(int num) {
	int newX = o.cursorX + num;
	if (newX < 0)
		newX = 0;
	else if (newX > o.screen[o.cursorY].size())
		newX = int(o.screen[o.cursorY].size());
	o.cursorX = newX;
}

static void incInpCur(int num) {
	int newInp = o.inputCursor + num;
	if (newInp < 0)
		newInp = 0;
	else {
		int len = get_length(o.command);
		if (len == -1)
			newInp = 1;
		else if (newInp > len) {
			newInp = len;
		}
	}
	o.inputCursor = newInp;
}

static size_t charIndexToByteOffset(const std::string& utf8line, int charIndex) {
	size_t offset = 0;
	int count = 0;
	const char* data = utf8line.data();
	size_t size = utf8line.size();

	while (offset < size && count < charIndex) {
		uint32_t cp;
		int len = decode_utf8(data + offset, &cp);
		if (len <= 0)
			break; // invalid UTF-8, stop early
		offset += len;
		count++;
	}
	return offset;
}

static void handleInput() {
	const std::u32string& typed = platform::getTypedInput();
	for (uint32_t ch : typed) {
		if (ch < 32)
			continue;
		char buf[4];
		int bytesWritten = encode_utf8(ch, buf);
		size_t index = charIndexToByteOffset(o.command, o.inputCursor);
		o.command.insert(index, buf, bytesWritten);
		incInpCur(1);
	}

	if (platform::isButtonTyped(platform::Button::Backspace)) {
		if (o.inputCursor > 0) {
			// erase one utf8 codepoint
			// FIXME: utf8 is multibyte
			
			size_t prevIndex = charIndexToByteOffset(o.command, o.inputCursor - 1);
			int len = codepoint_length(o.command.data() + prevIndex);
			o.command.erase(prevIndex, len);
			incInpCur(-1);
		}
	}
	if ((platform::isButtonHeld(platform::Button::LeftCtrl) || platform::isButtonPressed(platform::Button::LeftCtrl)) &&
		platform::isButtonPressed(platform::Button::V)) {
		const char* clip = platform::getClipboard(); // null-terminated UTF-8
		if (clip) {
			size_t insertIndex = charIndexToByteOffset(o.command, o.inputCursor);
			o.command.insert(insertIndex, clip);

			// Advance input cursor and visual cursor by codepoint count
			int cpCount = get_length(clip);
			incInpCur(cpCount);
		}
	}

	if (platform::isButtonTyped(platform::Button::Enter)) {
		o.command += '\n';
		processInput();
		o.shell->write(o.command.c_str(), o.command.size());
		if (o.flags.has(TermFlags::OUTPUT_RETURNS_INPUT)) {
			o.ignoreOutputCount = o.command.size();
		}
		o.command.clear();
		o.inputCursor = 0;
	}
}

bool gameLogic(float deltaTime) {
	int screenW, screenH;
	platform::getWindowSize(&screenW, &screenH);
	if (platform::isButtonPressed(platform::Button::F11))
		platform::setFullScreen(!platform::isFullScreen());

	// Assumes monospace font
	o.cols = screenH / o.fontSize;
	o.rows = 2 * screenW / o.fontSize;
	o.screen.resize(o.rows, o.cols);

	handleInput();

	o.shell->update();
	auto& buf = o.shell->getOutputBuffer();
	if (!buf.empty()) {
		processPartialOutputSegment(buf);
		buf.clear();
	}

	render(o.screen, screenW, screenH);
	renderCursor(o.cursorX + o.inputCursor, o.cursorY, deltaTime, screenW, screenH);
	return o.shell->isRunning();
}

void closeGame() {
	delete o.shell;
	stopRender();
}

void startGame() {
	o.shell = platform::launch();
	startRender(o.fontSize);

	o.flags = TermFlags::INPUT_ECHO | TermFlags::OUTPUT_ESCAPE_CODES;
#ifdef _WIN32
	o.flags |= TermFlags::INPUT_LF_TO_CRLF;
#else
	o.flags |= TermFlags::OUTPUT_RETURNS_INPUT;
#endif
}
