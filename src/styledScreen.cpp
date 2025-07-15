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

StyledLine StyledScreen::push_back(StyledLine line) {
	// If we are at the end, we need to move everything up, use memmove
	if (o.cursorY == cellsH) {
		memmove(screen, screen + cellsW, sizeof(StyledChar) * (cellsH - 1) * cellsW);
		// After shifting, set cursorY to the last valid row
		o.cursorY = cellsH - 1;
	}

	StyledLine currLine = at(o.cursorY);

	if (line.size() > cellsW) {
		line = StyledLine(line.data(), cellsW);
	}

	memcpy(currLine.data(), line.data(), sizeof(StyledChar) * line.size());
	// fill the rest of the line with spaces
	for (int i = line.size(); i < cellsW; ++i) {
		currLine[i] = StyledChar{U' ', TermColor::Default, TermColor::Default, TextAttribute::None};
	}
	return currLine;
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
