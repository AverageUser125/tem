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

static void insertCharAtCursor(char ch) {
	ensureLineExists(cursor.y);
	std::string& line = lines[cursor.y];
	if ((int)line.size() < cursor.x)
		line.resize(cursor.x, ' ');
	if (cursor.x == (int)line.size()) {
		line.push_back(ch);
	} else {
		line.insert(line.begin() + cursor.x, ch);
	}
	cursor.x++;
}

static void deleteCharBeforeCursor() {
	if (cursor.x > 0) {
		ensureLineExists(cursor.y);
		std::string& line = lines[cursor.y];
		if (cursor.x <= (int)line.size()) {
			line.erase(line.begin() + cursor.x - 1);
			cursor.x--;
		}
	}
}

static void deleteCharAtCursor() {
	ensureLineExists(cursor.y);
	std::string& line = lines[cursor.y];
	if (cursor.x < (int)line.size()) {
		line.erase(line.begin() + cursor.x);
	}
}

static void handleInput() {
	static std::string command;
	const std::string& typed = platform::getTypedInput();
	for (char ch : typed) {
		insertCharAtCursor(ch);
		command += ch;
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

	if (platform::isButtonPressed(platform::Button::Backspace))
		deleteCharBeforeCursor();

	if (platform::isButtonPressed(platform::Button::Delete))
		deleteCharAtCursor();

	if (platform::isButtonPressed(platform::Button::Enter)) {
		shell->write(command.c_str(), command.size());
		command.clear();
		shell->write("\r\n", 2);
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

	render(lines, scrollLineStart, fontSize, screenW, screenH);
	renderCursor(cursor.x, (cursor.y - scrollLineStart), deltaTime);
	return true;
}

void closeGame() {
	delete shell;
}

void startGame() {
	shell = platform::launch("cmd.exe /Q /K");
	startRender(fontSize);
}
