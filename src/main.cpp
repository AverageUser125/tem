#include "gameLogic.h"
#include <platform/window.h>
#include <platform/input.h>
#include <platform/shell.h>
#include <iostream>
#include "renderer.h"
#include "utf8.h"
#include "main.h"
#include "processText.h"

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

static void ensureLineExists(int y) {
	while ((int)o.screen.size() <= y)
		o.screen.emplace_back();
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

static void insertCodePointAtCursor(char32_t cp) {
	if (!o.flags.has(TermFlags::INPUT_ECHO)) {
		return;
	}
	ensureLineExists(o.cursorY);
	StyledLine& line = o.screen[o.cursorY];
	line.insert(line.begin() + o.cursorX, {cp});
	incCurX(1);
}

static void deleteCharBeforeCursor() {
	if (o.inputCursor <= 0)
		return;

	// Remove from o.command
	{
		size_t prevOffset = charIndexToByteOffset(o.command, o.inputCursor - 1);
		uint32_t cp;
		int len = decode_utf8(o.command.data() + prevOffset, &cp);
		if (len > 0)
			o.command.erase(prevOffset, len);
	}

	// Remove from visible line if echo is enabled
	if (o.flags.has(TermFlags::INPUT_ECHO)) {
		ensureLineExists(o.cursorY);
		StyledLine& line = o.screen[o.cursorY];
		if (o.cursorX >= 1) {
			line.erase(line.begin() + o.cursorX - 1);
		}
	}

	incInpCur(-1);
	incCurX(-1);
}

static void handleInput() {
	const std::u32string& typed = platform::getTypedInput();
	for (uint32_t ch : typed) {
		if (ch < 32)
			continue;
		insertCodePointAtCursor(ch);
		char buf[4];
		int bytesWritten = encode_utf8(ch, buf);
		size_t index = charIndexToByteOffset(o.command, o.inputCursor);
		o.command.insert(index, buf, bytesWritten);
		incInpCur(1);
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

			const char* p = clip;
			while (*p) {
				uint32_t cp;
				int len = decode_utf8(p, &cp);
				if (len <= 0)
					break; // invalid UTF-8 — stop
				insertCodePointAtCursor(cp);
				p += len;
			}
		}
	}
	if (platform::isButtonTyped(platform::Button::Left) && o.inputCursor > 0) {
		incInpCur(-1);
		incCurX(-1);
	}

	if (platform::isButtonTyped(platform::Button::Right) && o.inputCursor < get_length(o.command)) {
		incInpCur(1);
		incCurX(1);
	}

	if (platform::isButtonPressed(platform::Button::Home)) {
		incCurX(-o.inputCursor);
		o.inputCursor = 0;
	}
	if (platform::isButtonTyped(platform::Button::Backspace)) {
		deleteCharBeforeCursor();
	}

	if (platform::isButtonTyped(platform::Button::Enter)) {
		if (o.command.empty()) {
			o.shell->getOutputBuffer().emplace_back('\n');
		}
		o.command += '\n';
		processInput();
		o.shell->write(o.command.c_str(), o.command.size());
		if (o.flags.has(TermFlags::OUTPUT_RETURNS_INPUT)) {
			o.ignoreOutputCount = o.command.size();
		}
		o.command.clear();
		o.cursorX = 0;
		o.inputCursor = 0;
		o.cursorY++;
	}
}

bool gameLogic(float deltaTime) {
	int screenW, screenH;
	platform::getWindowSize(&screenW, &screenH);
	if (platform::isButtonPressed(platform::Button::F11))
		platform::setFullScreen(!platform::isFullScreen());

	handleInput();

	o.shell->update();
	auto& buf = o.shell->getOutputBuffer();
	if (!buf.empty()) {
		auto cleaned = processPartialOutputSegment(buf);
		appendNewLines(cleaned);
		buf.clear();
	}

	// Assumes monospace font
	o.cols = screenH / o.fontSize;
	o.rows = screenW / o.fontSize;

	int scrollLineStart = 0;
	int totalLines = int(o.screen.size());
	if (totalLines > o.cols)
		scrollLineStart = totalLines - o.cols;

	render(o.screen, scrollLineStart, screenW, screenH);
	renderCursor(o.cursorX, (o.cursorY - scrollLineStart), deltaTime, screenW, screenH);
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
