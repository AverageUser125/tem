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
	enum Value : uint16_t {
		NONE = 0,
		INPUT_LF_TO_CRLF = 1 << 0,
		INPUT_ECHO = 1 << 1,
		OUTPUT_WRAP_LINES = 1 << 2,
		OUTPUT_ESCAPE_CODES = 1 << 3,
		TRACK_FOCUS = 1 << 4,
		BRACKETED_PASTE = 1 << 5,
		SHOW_CURSOR = 1 << 6,
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
	int cursorX = 0, cursorY = 0;
	int rows = 0, cols = 0;
	TermFlags flags;
	int scrollbackOffset = 0;
};

extern Data o;