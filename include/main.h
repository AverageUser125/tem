#pragma once
#include <vector>
#include <string>
#include <cstdint>
#include "bitflags.hpp"
#include "renderer.h"

namespace platform
{
class Process;
}

class TermFlags {
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


struct Data {
	StyledScreen screen;
	std::string command;
	platform::Process* shell = nullptr;
	float fontSize = 16.0f;
	int cursorX = 0, cursorY = 0, inputCursor = 0;
	TermFlags flags;
};

extern Data o;