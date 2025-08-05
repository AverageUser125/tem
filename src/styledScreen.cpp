#include "styledScreen.h"
#include "styledScreen.h"
#include "styledScreen.h"
#include "styledScreen.h"
#include "styledScreen.h"
#include <platform/tools.h>
#include "main.h"
#include <iostream>

StyledChar makeStyledChar(char32_t ch) {
	return StyledChar{ch, o.procState.currFG, o.procState.currBG, o.procState.currAttr};
}

StyledScreen::StyledScreen() : cellsH(0), cellsW(0), screen(nullptr) {
}

StyledScreen::~StyledScreen() {
	delete[] screen;
}

void StyledScreen::resize(int width, int height) {
	if (cellsW == width && cellsH == height) {
		return; // No change in size
	}
	permaAssertDevelopement(width > 0 && height > 0);

	// Save old data
	StyledChar* oldScreen = screen;
	int oldW = cellsW;
	int oldH = cellsH;

	// Update to new size
	cellsW = width;
	cellsH = height;
	screen = new StyledChar[width * height];

	// Copy overlapping region from old screen
	int minW = (oldW > 0) ? std::min(oldW, width) : 0;
	int minH = (oldH > 0) ? std::min(oldH, height) : 0;
	for (int y = 0; y < minH; ++y) {
		for (int x = 0; x < minW; ++x) {
			screen[y * width + x] = oldScreen[y * oldW + x];
		}
	}

	// Fill new/empty cells with default StyledChar
	for (int y = 0; y < height; ++y) {
		for (int x = (y < minH ? minW : 0); x < width; ++x) {
			screen[y * width + x] = makeStyledChar(U' ');
		}
	}

	// Clean up old data
	delete[] oldScreen;
}

StyledLine StyledScreen::at(int idx) const {
	return StyledLine(screen + cellsW * idx, cellsW);
}

StyledLine StyledScreen::operator[](int index) const {
	return at(index);
}

int StyledScreen::get_width() const {
	return cellsW;
}

int StyledScreen::get_height() const {
	return cellsH;
}

int StyledScreen::size() const {
	return cellsH;
}

void StyledScreen::clear() {
	if (!screen)
		return;
	for (int y = 0; y < cellsH; ++y) {
		for (int x = 0; x < cellsW; ++x) {
			screen[y * cellsW + x] = makeStyledChar(U' ');
		}
	}
}

void StyledScreen::clearScrollback() {
	scrollbackBuffer.clear();
	clear();
}

StyledChar* StyledScreen::data() {
	return screen;
}

StyledChar& StyledScreen::atCursor() {
	// TODO: Handle wraping if cursorX exceeds width
	if (o.cursorX >= cellsW) {
		o.cursorX = cellsW - 1;
	}
	if (o.cursorY >= cellsH) {
		o.cursorY = cellsH - 1;
	}
	StyledChar& cursorChar = screen[o.cursorY * cellsW + o.cursorX];
	return cursorChar;
}

void StyledScreen::newLine() {
	// Save the top line to scrollback if we're at the bottom
	if (o.cursorY >= cellsH - 1) {
		// Copy the first line to scrollback
		std::vector<StyledChar> line(screen, screen + cellsW);
		scrollbackBuffer.push_back(std::move(line));
		if (scrollbackBuffer.size() > MaxScrollbackLines) {
			scrollbackBuffer.pop_front();
		}
		// Scroll all lines up
		memmove(screen, screen + cellsW, sizeof(StyledChar) * cellsW * (cellsH - 1));
		// Clear the last line
		for (int x = 0; x < cellsW; ++x) {
			screen[(cellsH - 1) * cellsW + x] = makeStyledChar(U' ');
		}
		o.cursorY = cellsH - 1;
	} else {
		o.cursorY++;
	}
}

std::vector<tcb::span<StyledChar>> StyledScreen::getSnapshotView(int scrollbackOffset) {
	std::vector<tcb::span<StyledChar>> snapshot;
	snapshot.reserve(cellsH);

	// Clamp scrollbackOffset to valid range
	int maxScroll = static_cast<int>(scrollbackBuffer.size());
	if (scrollbackOffset < 0)
		scrollbackOffset = 0;
	if (scrollbackOffset > maxScroll)
		scrollbackOffset = maxScroll;

	// The first line to show is at: scrollbackBuffer.size() - scrollbackOffset
	int firstLineIdx = maxScroll - scrollbackOffset;

	for (int i = 0; i < cellsH; ++i) {
		int lineIdx = firstLineIdx + i;
		if (lineIdx < 0) {
			// Not enough scrollback, show blank line
			snapshot.emplace_back();
		} else if (lineIdx < maxScroll) {
			// From scrollback buffer
			std::vector<StyledChar>& vec = scrollbackBuffer[lineIdx];
			snapshot.emplace_back(vec.data(), vec.size());
		} else {
			// From current screen
			int screenLine = lineIdx - maxScroll;
			if (screenLine < cellsH) {
				snapshot.emplace_back(screen + screenLine * cellsW, cellsW);
			} else {
				snapshot.emplace_back();
			}
		}
	}
	return snapshot;
}

ScreenState StyledScreen::getScreenState() const {
	ScreenState state;
	state.width = cellsW;
	state.height = cellsH;
	state.scrollback = scrollbackBuffer;
	state.screen.assign(screen, screen + cellsW * cellsH);
	state.cursorX = o.cursorX;
	state.cursorY = o.cursorY;
	return state;
}

void StyledScreen::setScreenState(const ScreenState& state) {
	// Resize if needed
	if (cellsW != state.width || cellsH != state.height) {
		resize(state.width, state.height);
	}
	scrollbackBuffer = state.scrollback;
	if (state.screen.size() == static_cast<size_t>(cellsW * cellsH)) {
		std::copy(state.screen.begin(), state.screen.end(), screen);
	} else {
		// Fallback: clear if size mismatch
		clear();
	}
	o.cursorX = state.cursorX;
	o.cursorY = state.cursorY;
}

std::string StyledScreen::lineToString(const StyledLine& line) {
	std::string result;
	for (int i = 0; i < line.size(); ++i) {
		if (line[i].ch == U'\0') {
			result += ' '; // Replace null characters with spaces
		} else {
			result += static_cast<char>(line[i].ch);
		}
	}
	return result;
}

std::string StyledScreen::lineToString(const std::vector<StyledChar>& line) {
	StyledLine sline = StyledLine((StyledChar*)line.data(), (size_t)line.size());
	return lineToString(sline);
}
