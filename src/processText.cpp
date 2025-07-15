#include <cstdint>
#include "main.h"
#include "processText.h"
#include "utf8.h"
#include "bitflags.hpp"
#include <string_view>
#include <iostream>
#include <string>
#include <cstdio>
#include <cstring>
#include <platform/window.h>
#include <charconv>
#include <stdexcept>
#include "styledScreen.h"

namespace std
{
static int stoi(std::string_view sv) {
	int out;
	if (sv.empty() || sv.data()[0] == '\0') {
		throw std::runtime_error("stoi failed");
	}
	const std::from_chars_result result = std::from_chars(sv.data(), sv.data() + sv.size(), out);
	if (result.ec == std::errc::invalid_argument || result.ec == std::errc::result_out_of_range) {
		throw std::runtime_error("stoi failed");
	}
	return out;
}
}

static std::vector<std::string_view> split(const std::string_view& str, char delimiter) {
	std::vector<std::string_view> parts;
	size_t start = 0;
	while (true) {
		size_t pos = str.find(delimiter, start);
		if (pos == std::string_view::npos) {
			parts.emplace_back(str.substr(start));
			break;
		}
		parts.emplace_back(str.substr(start, pos - start));
		start = pos + 1;
	}
	return parts;
}

static void applySGRColor(std::string_view codeStr) {
	auto codes = split(codeStr, ';');

	for (auto& part : codes) {
		int code = 0;
		try {
			code = std::stoi(part);
		} catch (...) {
			continue; // ignore invalid input, but continue with others
		}

		switch (code) {
		case 0:
			o.procState.currFG = TermColor::Default;
			o.procState.currBG = TermColor::Default;
			o.procState.currAttr = TextAttribute::None;
			continue;
		case 1:
			o.procState.currAttr |= TextAttribute::Bold;
			continue;
		case 3:
			o.procState.currAttr |= TextAttribute::Italic;
			continue;
		case 4:
			o.procState.currAttr |= TextAttribute::Underline;
			continue;
		case 7:
			o.procState.currAttr |= TextAttribute::Inverse;
			continue;
		}

		int normalized = code;
		if ((code >= 40 && code <= 47) || (code >= 100 && code <= 107))
			normalized -= 10;

		TermColor color;
		switch (normalized) {
		case 30:
			color = TermColor::Black;
			break;
		case 31:
			color = TermColor::Red;
			break;
		case 32:
			color = TermColor::Green;
			break;
		case 33:
			color = TermColor::Yellow;
			break;
		case 34:
			color = TermColor::Blue;
			break;
		case 35:
			color = TermColor::Magenta;
			break;
		case 36:
			color = TermColor::Cyan;
			break;
		case 37:
			color = TermColor::White;
			break;
		case 90:
			color = TermColor::BrightBlack;
			break;
		case 91:
			color = TermColor::BrightRed;
			break;
		case 92:
			color = TermColor::BrightGreen;
			break;
		case 93:
			color = TermColor::BrightYellow;
			break;
		case 94:
			color = TermColor::BrightBlue;
			break;
		case 95:
			color = TermColor::BrightMagenta;
			break;
		case 96:
			color = TermColor::BrightCyan;
			break;
		case 97:
			color = TermColor::BrightWhite;
			break;
		default:
			color = TermColor::Default;
			break;
		}

		if ((code >= 30 && code <= 37) || (code >= 90 && code <= 97))
			o.procState.currFG = color;
		else if ((code >= 40 && code <= 47) || (code >= 100 && code <= 107))
			o.procState.currBG = color;
	}
}

static TermFlags termFlagFromNumber(int code) {
	switch (code) {
	case 1:
		return TermFlags::INPUT_LF_TO_CRLF; // Map to input LF->CRLF
	case 2:
		return TermFlags::INPUT_ECHO; // Echo input characters
	case 7:
		return TermFlags::OUTPUT_WRAP_LINES; // Auto-wrap lines enabled

	case 25:
		// Show cursor
		// return TermFlags::SHOW_CURSOR;
		return TermFlags::NONE;

	case 1049:
	case 1047:
		// Alternate screen buffer
		// return TermFlags::ALT_SCREEN_BUFFER;
		return TermFlags::NONE;

	case 1048:
		// Save cursor position
		// return TermFlags::SAVE_CURSOR;
		return TermFlags::NONE;

	default:
		return TermFlags::NONE; // Unknown or unsupported code
	}
}

static void handleDECPrivateMode(std::string_view params, char finalChar) {
	// Example: params = "?7" or "?25" etc.
	if (params.empty() || params[0] != '?') {
		return; // Not a DEC private mode sequence
	}

	std::string_view modeStr = params.substr(1);

	int mode = 0;
	try {
		mode = std::stoi(modeStr);
	} catch (...) {
		return;
	}

	TermFlags::Value flag = termFlagFromNumber(mode);

	if (flag == TermFlags::NONE) {
		return;
	}

	if (finalChar == 'h') {
		// Set the flag
		o.flags |= flag;
	} else if (finalChar == 'l') {
		// Clear the flag
		o.flags &= ~flag;
	}
}

static void handleEraseInDisplay(int mode) {
	switch (mode) {
	case 0: { // Erase from cursor to end of screen
		for (int y = o.cursorY; y < o.rows; ++y) {
			StyledLine& line = o.screen[y];
			int start = (y == o.cursorY) ? o.cursorX : 0;
			for (size_t x = start; x < line.size(); ++x) {
				line[x] = makeStyledChar(U' ');
			}
		}
		break;
	}
	case 1: { // Erase from start to cursor
		for (int y = 0; y <= o.cursorY; ++y) {
			StyledLine& line = o.screen[y];
			int end = (y == o.cursorY) ? o.cursorX : static_cast<int>(line.size());
			for (int x = 0; x < end && x < static_cast<int>(line.size()); ++x) {
				line[x] = makeStyledChar(U' ');
			}
		}
		break;
	}
	case 2: { // Erase entire screen
		o.screen.clear();
		o.cursorY = 0;
		o.cursorX = 0;
		o.inputCursor = 0;
		break;
	}
	case 3: { // Erase scrollback buffer
		o.screen.clear();
		break;
	}
	default:
		// Unknown mode, ignore
		break;
	}
}

static void handleCSI() {
	std::string& csiData = o.procState.escBuf;
	char type = csiData.back();
	csiData.pop_back();
	std::cout << "[" << type << "]'" << csiData << "'\n";

	switch (type) {
	case 'm': {
		applySGRColor(csiData);
		break;
	}
	case 'G': {
		int col = std::stoi(csiData);
		o.cursorX = col;
		break;
	}
	case 'A': {
		// TODO: more protections
		int moveUpBy = std::stoi(csiData);
		if (o.cursorY > moveUpBy) {
			o.cursorY -= moveUpBy;
		} else {
			o.cursorY = 0;
		}
		break;
	}
	case 'B': {
		// TODO: more protections
		int moveDownBy = std::stoi(csiData);
		o.cursorY += moveDownBy;
		break;
	}
	case 'C': {
		// TODO: more protections
		int moveForwardBy = std::stoi(csiData);
		o.cursorX += moveForwardBy;
		break;
	}
	case 'D': {
		// TODO: more protections
		int moveBackwardsBy = std::stoi(csiData);
		o.cursorX -= moveBackwardsBy;
		break;
	}
	case 'I': {
		int count = 1;
		try {
			count = std::stoi(csiData);
		} catch (...) {
			count = 1;
		}
		StyledChar blankChar = makeStyledChar(U' ');
		StyledLine& line = o.screen[o.cursorY];
		// TODO: Fix this, it should insert blanks at the cursor position
		//line.insert(line.begin() + o.cursorX, count, blankChar);

		break;
	}
	case 'l':
	case 'h': {
		handleDECPrivateMode(csiData, type);
		break;
	}
	case 'H': {
		if (csiData.empty()) {
			o.cursorX = 0;
			o.cursorY = 0;
			break;
		}
		auto params = split(csiData, ';');
		try {
			o.cursorX = std::stoi(params[1]);
			o.cursorY = std::stoi(params[0]);
		} catch (...) {
		}
		// TODO: FIX THE CURSOR, split it into absolute and relative cursors
		// or a scrollBarY and a relative cursor, just something.
		break;
	}
	case 'K': {
		int mode = 0;
		try {
			mode = std::stoi(csiData);
		} catch (...) {
		}
		if (mode == 0) {
			StyledLine& line = o.screen[o.cursorY];
			if (!line.empty()) {
				// TODO: erase in line
				//line.erase(line.begin() + o.cursorX, line.end());
			}
		}
		break;
	}
	case 'J': {
		int mode = 0;
		if (!csiData.empty()) {
			try {
				mode = std::stoi(csiData);
			} catch (...) {
			}
		}
		handleEraseInDisplay(mode);
		break;
	}
	default: {
		break;
	}
	}

	csiData.clear();
}

static void handleOSC() {
	std::string& oscData = o.procState.escBuf;
	size_t semicolonPos = oscData.find(';');
	if (semicolonPos == std::string_view::npos) {
		// No parameter found — treat whole as default OSC command (e.g., title)
		platform::setWindowTitle(oscData.c_str());
		return;
	}

	std::string_view param = std::string_view(oscData.data(), semicolonPos);
	std::string_view content = std::string_view(oscData.data() + semicolonPos + 1, oscData.size() - semicolonPos - 1);

	int paramNum = 0;
	try {
		paramNum = std::stoi(param);
	} catch (...) {
		return;
	}

	switch (paramNum) {
	case 0:
	case 2:
		// Set both icon name and window title (0) or window title only (2)
		platform::setWindowTitle(content.data());
		break;

	case 1:
		// Set icon name only - you could implement if you want
		// setIconName(std::string(content));
		break;

	case 52:
		// Clipboard operations (OSC 52) could be handled here
		// handleClipboard(content);
		break;

	default:
		// Unknown/unhandled OSC command — ignore or log
		break;
	}
	oscData.clear();
}

void processPartialOutputSegment(const std::vector<char>& inputSegment) {
	size_t skip = std::min(static_cast<size_t>(o.ignoreOutputCount), inputSegment.size());
	o.ignoreOutputCount -= static_cast<int>(skip);
	// Insert only the part that remains
	o.procState.leftover.insert(o.procState.leftover.end(), inputSegment.begin() + skip, inputSegment.end());

	std::vector<StyledChar> currentLine;
	size_t i = 0;
	const char* utf8Buf = nullptr;
	const char* utf8End = nullptr;
	std::vector<char> utf8Accum;

	while (i < o.procState.leftover.size()) {
		char c = o.procState.leftover[i];

		switch (o.procState.state) {
		case ProcState::None:
			switch (c) {
			case '\033': { // ESC
				if (!currentLine.empty()) {
					o.screen.push_back(currentLine);
					currentLine.clear();
				}
				o.procState.state = ProcState::SawESC;
				i++;
				break;
			}
			case '\r': // Carriage Return
				o.procState.state = ProcState::SawCR;
				i++;
				break;

			case '\f': // Form Feed
				o.screen.clear();
				o.cursorX = 0;
				o.cursorY = 0;
				currentLine.clear();
				i++;
				break;

			case '\t': // Tab
				for (int t = 0; t < 4; ++t) {
					currentLine.push_back(makeStyledChar(U' '));
					o.cursorX++;
				}
				i++;
				break;

			case '\b': // Backspace
				if (!currentLine.empty() && o.cursorX > 0) {
					currentLine.pop_back();
					o.cursorX--;
				}
				i++;
				break;

			case '\n': {
				// Commit the current line and reset
				if (!currentLine.empty() || o.cursorX > 0) {
					o.screen.push_back(currentLine);
					currentLine.clear();
				} else {
					// Even if empty, push an empty line for a bare newline
					o.screen.push_back({});
				}
				o.cursorX = 0;
				o.cursorY++;
				i++;
				break;
			}
			default: {
				// Accumulate UTF-8 bytes for decoding
				utf8Accum.push_back(c);
				i++;
				// Try to decode as much as possible
				utf8Buf = utf8Accum.data();
				utf8End = utf8Buf + utf8Accum.size();
				while (utf8Buf < utf8End) {
					uint32_t cp;
					int len = decode_utf8(utf8Buf, &cp);
					if (len <= 0 || utf8Buf + len > utf8End)
						break;
					utf8Buf += len;

					currentLine.push_back(makeStyledChar(cp));
					o.cursorX++;
				}
				// Remove processed bytes from utf8Accum
				if (utf8Buf > utf8Accum.data()) {
					utf8Accum.erase(utf8Accum.begin(), utf8Accum.begin() + (utf8Buf - utf8Accum.data()));
				}
				break;
			}
			}
			break;

		case ProcState::SawCR:
			if (c == '\n') {
			} else {
				// Lone CR, treat as line break
				if (!currentLine.empty() || o.cursorX > 0) {
					o.screen.push_back(currentLine);
					currentLine.clear();
				} else {
					o.screen.push_back({});
				}
				o.cursorX = 0;
				o.cursorY++;
				// Do not increment i, reprocess this character
			}
			o.procState.state = ProcState::None;
			break;

		case ProcState::SawESC:
			if (c == '[') {
				o.procState.state = ProcState::SawCSIBracket;
			} else if (c == ']') {
				o.procState.state = ProcState::SawOSCBracket;
			} else {
				o.procState.state = ProcState::None;
			}
			i++;
			break;

		case ProcState::SawCSIBracket: {
			o.procState.escBuf += c;
			if ((unsigned char)c >= 0x40 && (unsigned char)c <= 0x7E) {
				o.procState.state = ProcState::None;
				handleCSI();
			}
			i++;
			break;
		}
		case ProcState::SawOSCBracket: {
			if (c == '\033') {
				o.procState.state = ProcState::SawOSCBracketAndESC; // saw ESC inside OSC
			} else if (c == 0x07) {
				// BEL terminator found — end of OSC
				o.procState.state = ProcState::None;
				handleOSC();
				o.procState.escBuf.clear();
			} else {
				o.procState.escBuf += c;
			}
			i++;
			break;
		}

		case ProcState::SawOSCBracketAndESC: {
			if (c == '\\') {
				// ESC \ terminator found — end of OSC
				o.procState.state = ProcState::None;
				handleOSC();
				o.procState.escBuf.clear();
			} else {
				// False alarm, ESC wasn't terminator - push ESC + current char to buffer
				o.procState.escBuf += '\033';
				o.procState.escBuf += c;
				o.procState.state = ProcState::SawOSCBracket;
			}
			i++;
			break;
		}
		}
	}

	if (i > 0) {
		o.procState.leftover.erase(o.procState.leftover.begin(), o.procState.leftover.begin() + i);
	}

	// Save partial line (if any)
	if (!currentLine.empty()) {
		o.screen.push_back(currentLine);
	}
}

void processInput() {
	if (o.flags.has(TermFlags::INPUT_LF_TO_CRLF)) {
		size_t pos = 0;
		while ((pos = o.command.find('\n', pos)) != std::string::npos) {
			o.command.replace(pos, 1, "\r\n");
			pos += 2;
		}
	}
}
