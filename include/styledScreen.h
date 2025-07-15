#pragma once
#include "bitflags.hpp"
#include "span.hpp"
#include <vector>

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

using StyledLine = tcb::span<StyledChar>;

class StyledScreen {
  public:
	StyledScreen();
	~StyledScreen();
	void resize(int width, int height);
	StyledLine at(int idx) const;
	StyledLine back() const;
	StyledLine operator[](int index) const;
	void clear();
	int get_width() const;
	int get_height() const;
	int size() const;
	StyledChar* data() const;
	void append_line(StyledLine line);
	void push_back(StyledLine line);

	static std::string line_to_string(const StyledLine& line);
	static std::string line_to_string(const std::vector<StyledChar>& line);

  private:
	StyledChar* screen;
	int cellsW;
	int cellsH;
};

StyledChar makeStyledChar(char32_t ch);