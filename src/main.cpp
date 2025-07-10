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
	else if (newX > get_length(o.lines[o.cursorY]))
		newX = int(get_length(o.lines[o.cursorY]));
	o.cursorX = newX;
}

static void incInpCur(int num) {
	int newInp = o.inputCursor + num;
	if (newInp < 0)
		newInp = 0;
	else if (newInp > get_length(o.command))
		newInp = int(get_length(o.command));
	o.inputCursor = newInp;
}

static void ensureLineExists(int y) {
	while ((int)o.lines.size() <= y)
		o.lines.emplace_back();
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

static void insertCodePointAtCursor(char32_t cp, std::string& command) {
	ensureLineExists(o.cursorY);
	std::string& line = o.lines[o.cursorY];
	size_t byteOffset = charIndexToByteOffset(line, o.cursorX);

	// encode cp to UTF-8 bytes using your encode_utf8 function
	char buf[4];
	int bytesWritten = encode_utf8(cp, buf);
	if (o.flags.has(TermFlags::INPUT_ECHO)) {
		line.insert(byteOffset, buf, bytesWritten);
		o.cursorX++;
	}

	byteOffset = charIndexToByteOffset(o.command, o.inputCursor);
	command.insert(byteOffset, buf, bytesWritten);
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
		std::string& line = o.lines[o.cursorY];
		size_t prevOffset = charIndexToByteOffset(line, o.cursorX - 1);
		uint32_t cp;
		int len = decode_utf8(line.data() + prevOffset, &cp);
		if (len > 0)
			line.erase(prevOffset, len);
	}

	incInpCur(-1);
	incCurX(-1);
}

static void handleInput() {
	const std::u32string& typed = platform::getTypedInput();
	for (uint32_t ch : typed) {
		if (ch < 32)
			continue;
		insertCodePointAtCursor(ch, o.command);
		incInpCur(1);
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
	// TODO: make it remove from command
	if (platform::isButtonTyped(platform::Button::Backspace)) {
		deleteCharBeforeCursor();
	}

	if (platform::isButtonTyped(platform::Button::Enter)) {
		o.command += '\n';
		processInput();
		o.shell->write(o.command.c_str(), o.command.size());
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
		auto cleaned = processPartialInputSegment(buf);
		appendNewLines(cleaned);
		buf.clear();
	}

	int scrollLineStart = 0;
	int totalLines = int(o.lines.size());
	int visibleLines = screenH / o.fontSize;
	if (totalLines > visibleLines)
		scrollLineStart = totalLines - visibleLines;

	render(o.lines, scrollLineStart, screenW, screenH);
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

	o.flags = TermFlags::INPUT_ECHO | TermFlags::OUTPUT_CRLF_TO_LF | TermFlags::OUTPUT_ESCAPE_CODES;
	#ifdef _WIN32
	o.flags |= TermFlags::INPUT_LF_TO_CRLF;
	#else 
	#endif
}
