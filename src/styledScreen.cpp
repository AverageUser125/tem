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

StyledChar& StyledScreen::atCursor() {
	if (o.cursorY >= cellsH) {
		int offset = o.cursorY - cellsH + 1; // +1 for 0-based indexing
		// Move rows up by offset
		for (int y = 0; y < cellsH - offset; ++y) {
			for (int x = 0; x < cellsW; ++x) {
				screen[y * cellsW + x] = screen[(y + offset) * cellsW + x];
			}
		}
		// Clear the new bottom rows
		for (int y = cellsH - offset; y < cellsH; ++y) {
			for (int x = 0; x < cellsW; ++x) {
				screen[y * cellsW + x] = makeStyledChar(U' ');
			}
		}
		o.cursorY = cellsH - 1; // Move cursor to last row
	}
	// Ensure cursorX is within bounds
	// TODO: support wrapping
	o.cursorX = std::max(0, std::min(o.cursorX, cellsW - 1));


	StyledChar& cursorChar = screen[o.cursorY * cellsW + o.cursorX];
	return cursorChar;
}

std::string StyledScreen::line_to_string(const StyledLine& line) {
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

std::string StyledScreen::line_to_string(const std::vector<StyledChar>& line) {
	StyledLine sline = StyledLine((StyledChar*)line.data(), (size_t)line.size());
	return line_to_string(sline);
}
