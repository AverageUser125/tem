#pragma once
#include <vector>
#include <string>
#include <cstdint>
#include <string_view>
#include "bitflags.hpp"
#include "styledScreen.h"

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
		OUTPUT_RETURNS_INPUT = 1 << 2,
		OUTPUT_WRAP_LINES = 1 << 3,
		OUTPUT_ESCAPE_CODES = 1 << 4,
		TRACK_FOCUS = 1 << 5,
	};

  private:
	DEFINE_BITFLAGS(TermFlags);
};

enum class ProcState : uint8_t {
	None,
	SawESC,
	SawCSIBracket,
	SawOSCBracket,
	SawOSCBracketAndESC,
	SawCR,
};

struct InputProcessorState {
	std::string leftover;
	std::string escBuf;
	ProcState state = ProcState::None;
	TermColor currFG = TermColor::DefaultForeGround();
	TermColor currBG = TermColor::DefaultBackGround();
	TextAttribute currAttr = TextAttribute::None;
};

struct Data {
	InputProcessorState procState;
	std::string command;
	StyledScreen screen;
	platform::Process* shell = nullptr;
	float fontSize = 16.0f;
	int cursorX = 0, cursorY = 0, inputCursor = 0;
	int rows = 0, cols = 0;
	int ignoreOutputCount = 0;
	TermFlags flags;
	bool showCursor = true;
};

extern Data o;