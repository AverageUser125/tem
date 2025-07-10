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
static float fontSize = 16.0f;
static ivec2 cursor;

// Call this when new output is written
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
			cursor.x = 0;
			cursor.y++;
		} else {
			p += len;
		}
	}

	if (lineStart < p) {
		std::string partial(lineStart, p - lineStart);

		if (!lines.empty() && !lines.back().empty()) {
			lines.back() += partial;
		} else {
			lines.emplace_back(std::move(partial));
		}
		cursor.x = int(lines.back().size());
		cursor.y = int(lines.size() - 1);
	}

	processedBytes = size_t(p - data);
}

static void ensureLineExists(int y) {
	while ((int)lines.size() <= y)
		lines.emplace_back();
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
	ensureLineExists(cursor.y);
	std::string& line = lines[cursor.y];
	size_t byteOffset = charIndexToByteOffset(line, cursor.x);

	// encode cp to UTF-8 bytes using your encode_utf8 function
	char buf[4];
	int bytesWritten = encode_utf8(cp, buf);

	line.insert(byteOffset, buf, bytesWritten);
	command.insert(command.size(), buf, bytesWritten);
	cursor.x++;
}

static void deleteCharBeforeCursor() {
	if (cursor.x > 0) {
		ensureLineExists(cursor.y);
		std::string& line = lines[cursor.y];

		size_t byteOffset = charIndexToByteOffset(line, cursor.x);
		size_t prevByteOffset = 0;
		// find byte offset of previous char:
		{
			size_t offset = 0;
			int count = 0;
			const char* data = line.data();
			while (offset < line.size() && count < cursor.x - 1) {
				uint32_t cp;
				int len = decode_utf8(data + offset, &cp);
				if (len <= 0)
					break;
				offset += len;
				count++;
			}
			prevByteOffset = offset;
		}

		line.erase(prevByteOffset, byteOffset - prevByteOffset);
		cursor.x--;
	}
}

static void handleInput() {
	static std::string command;
	const std::u32string& typed = platform::getTypedInput();
	for (uint32_t ch : typed) {
		if (ch < 32)
			continue;
		insertCodePointAtCursor(ch, command);
	}
	if (platform::isButtonPressed(platform::Button::Left) && cursor.x > 0)
		cursor.x--;

	if (platform::isButtonPressed(platform::Button::Right))
		cursor.x++;

	if (platform::isButtonPressed(platform::Button::Up) && cursor.y > 0) {
		cursor.y--;
		cursor.x = std::min(cursor.x, (int)lines[cursor.y].size());
	}

	if (platform::isButtonPressed(platform::Button::Down) && cursor.y + 1 < (int)lines.size()) {
		cursor.y++;
		cursor.x = std::min(cursor.x, (int)lines[cursor.y].size());
	}

	if (platform::isButtonPressed(platform::Button::Home))
		cursor.x = 0;

	if (platform::isButtonPressed(platform::Button::End) && cursor.y < (int)lines.size())
		cursor.x = (int)lines[cursor.y].size();

	// TODO: make it remove from command
	if (platform::isButtonPressed(platform::Button::Backspace)) {
		//deleteCharBeforeCursor();
	}

	if (platform::isButtonPressed(platform::Button::Enter)) {
		if (!command.empty()) {
			shell->write(command.c_str(), command.size());
		} else {
			insertCodePointAtCursor('\n', command);
		}
		command.clear();
		#ifdef _WIN32
		shell->write("\r\n", 2);
		#else
		shell->write("\n", 1);
		#endif
		cursor.x = 0;
		cursor.y++;
	}
}

bool gameLogic(float deltaTime) {
	int screenW, screenH;
	platform::getWindowSize(&screenW, &screenH);
	glViewport(0, 0, screenW, screenH);
	glClear(GL_COLOR_BUFFER_BIT);

	if (platform::isButtonPressed(platform::Button::F11))
		platform::setFullScreen(!platform::isFullScreen());

	handleInput();

	shell->update();
	appendNewLines(shell->getOutputBuffer());

	int scrollLineStart = 0;
	int totalLines = int(lines.size());
	int visibleLines = screenH / fontSize;
	if (totalLines > visibleLines)
		scrollLineStart = totalLines - visibleLines;

	render(lines, scrollLineStart, screenW, screenH);
	renderCursor(cursor.x, (cursor.y - scrollLineStart), deltaTime, screenW, screenH);
	return shell->isRunning();
}

void closeGame() {
	delete shell;
	stopRender();
}

void startGame() {
	#ifdef _WIN32
	shell = platform::launch("cmd.exe /Q /K");
	#else
	shell = platform::launch("/bin/bash");
	#endif
	startRender(fontSize);
}
