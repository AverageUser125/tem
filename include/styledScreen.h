#pragma once
#include "bitflags.hpp"
#include "span.hpp"
#include <vector>

struct TermColor {
	unsigned char r;
	unsigned char g;
	unsigned char b;

	constexpr TermColor(int r_, int g_, int b_)
		: r(static_cast<unsigned char>(r_)), g(static_cast<unsigned char>(g_)), b(static_cast<unsigned char>(b_)) {
	}

	constexpr TermColor(unsigned char r_, unsigned char g_, unsigned char b_) : r(r_), g(g_), b(b_) {
	}

	inline constexpr static TermColor DefaultBackGround() {
		return TermColor{0, 0, 0};
	}

	inline constexpr static TermColor DefaultForeGround() {
		return TermColor{255, 255, 255};
	}

	friend constexpr bool operator==(const TermColor& lhs, const TermColor& rhs) {
		return lhs.r == rhs.r && lhs.g == rhs.g && lhs.b == rhs.b;
	}

	friend constexpr bool operator!=(const TermColor& lhs, const TermColor& rhs) {
		return !(lhs == rhs);
	}
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
	char32_t ch = U' '; // Unicode codepoint
	TermColor fg = TermColor::DefaultForeGround();
	TermColor bg = TermColor::DefaultBackGround();
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
	StyledChar& atCursor();

	static std::string line_to_string(const StyledLine& line);
	static std::string line_to_string(const std::vector<StyledChar>& line);

  private:
	StyledChar* screen;
	int cellsW;
	int cellsH;
};

StyledChar makeStyledChar(char32_t ch);