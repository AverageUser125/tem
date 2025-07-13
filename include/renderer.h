#pragma once
#include "bitflags.hpp"
#include <string>
#include <vector>
#include <string_view>
#include <cstdint>

enum class TermColor : uint8_t {
	Default,
	Black,
	Red,
	Green,
	Yellow,
	Blue,
	Magenta,
	Cyan,
	White,
	BrightBlack,
	BrightRed,
	BrightGreen,
	BrightYellow,
	BrightBlue,
	BrightMagenta,
	BrightCyan,
	BrightWhite
};

struct TextAttribute {
  public:
	enum Value : uint8_t {
		None = 0,
		Bold = 1 << 0,
		Italic = 1 << 1,
		Underline = 1 << 2,
		Inverse = 1 << 3,
	};

	DEFINE_BITFLAGS(TextAttribute);
};

struct StyledChar {
	char32_t ch; // Unicode codepoint
	TermColor fg = TermColor::Default;
	TermColor bg = TermColor::Default;
	TextAttribute attr = TextAttribute::None;
};
using StyledLine = std::vector<StyledChar>;
using StyledScreen = std::vector<StyledLine>;

void startRender(float fontSize);
void render(StyledScreen& screen, int startLineIndex, int screenW, int screenH);
void renderCursor(int cursorX, int cursorY, float deltaTime, int screenW, int screenH);
void stopRender();