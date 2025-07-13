#pragma once
#include <vector>
#include <string>
#include <cstdint>
#include "bitflags.hpp"

namespace platform
{
class Process;
}

struct TermFlags {
  public:
	enum Value : uint8_t {
		NONE = 0,
		INPUT_LF_TO_CRLF = 1 << 0,
		INPUT_ECHO = 1 << 1,
		OUTPUT_WRAP_LINES = 1 << 2,
		OUTPUT_ESCAPE_CODES = 1 << 3,
	};

  private:
	DEFINE_BITFLAGS(TermFlags);
};

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

enum class ProcState : uint8_t {
	None,
	SawESC,
	SawESCBracket,
	SawCR,
};

struct StyledChar {
	char32_t ch; // Unicode codepoint
	TermColor fg = TermColor::Default;
	TermColor bg = TermColor::Default;
	TextAttribute attr = TextAttribute::None;
};
using StyledLine = std::vector<StyledChar>;
using StyledScreen = std::vector<StyledLine>;

struct InputProcessorState {
	std::string leftover;
	std::string escBuf;
	ProcState state = ProcState::None;
	TermColor currFG = TermColor::Default;
	TermColor currBG = TermColor::Default;
	TextAttribute currAttr = TextAttribute::None;
};

struct Data {
	InputProcessorState procState;
	std::string command;
	StyledScreen screen;
	platform::Process* shell = nullptr;
	float fontSize = 16.0f;
	int cursorX = 0, cursorY = 0, inputCursor = 0;
	TermFlags flags;
};

extern Data o;