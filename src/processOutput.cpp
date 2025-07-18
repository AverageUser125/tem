#include <cstdint>
#include "main.h"
#include "processOutput.h"
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
#include <platform/tools.h>

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

static int stoi(std::string_view sv, int defaultVal) {
	int out;
	if (sv.empty() || sv.data()[0] == '\0') {
		return defaultVal;
	}
	const std::from_chars_result result = std::from_chars(sv.data(), sv.data() + sv.size(), out);
	if (result.ec == std::errc::invalid_argument || result.ec == std::errc::result_out_of_range) {
		throw std::runtime_error("stoi failed");
	}
	return out;
}
}

namespace
{
static void setFlag(TermFlags::Value flag, bool enable) {
	if (enable) {
		o.flags |= flag;
	} else {
		o.flags &= ~flag;
	}
};

std::vector<std::string_view> split(const std::string_view& str, char delimiter) {
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

constexpr TermColor kBasicColors[16] = {
	TermColor(0, 0, 0),		  // black
	TermColor(128, 0, 0),	  // red
	TermColor(0, 128, 0),	  // green
	TermColor(128, 128, 0),	  // yellow
	TermColor(0, 0, 128),	  // blue
	TermColor(128, 0, 128),	  // magenta
	TermColor(0, 128, 128),	  // cyan
	TermColor(192, 192, 192), // white
	TermColor(128, 128, 128), // bright black (gray)
	TermColor(255, 0, 0),	  // bright red
	TermColor(0, 255, 0),	  // bright green
	TermColor(255, 255, 0),	  // bright yellow
	TermColor(0, 0, 255),	  // bright blue
	TermColor(255, 0, 255),	  // bright magenta
	TermColor(0, 255, 255),	  // bright cyan
	TermColor(255, 255, 255)  // bright white
};

TermColor colorFrom256(int index) {
	// 256-color xterm palette: 16..231 is a 6x6x6 color cube
	// index: 0-255
	if (index >= 16 && index <= 231) {
		int idx = index - 16;
		int r = (idx / 36) % 6;
		int g = (idx / 6) % 6;
		int b = idx % 6;
		return TermColor(r * 51, g * 51, b * 51);
	}
	// 232..255: grayscale ramp
	if (index >= 232 && index <= 255) {
		int gray = 8 + (index - 232) * 10;
		return TermColor(gray, gray, gray);
	}
	// 0..15: standard colors (fallback to simple mapping)
	if (index >= 0 && index < 16)
		return kBasicColors[index];
	// fallback: black
	return TermColor(0, 0, 0);
}

void applySGRColor(std::string_view codeStr) {
	auto codes = split(codeStr, ';');

	for (size_t i = 0; i < codes.size(); ++i) {
		int code = 0;
		try {
			if (codes[i].empty()) {
				code = 0; // Default code
			} else {
				code = std::stoi(codes[i]);
			}
		} catch (...) {
			continue;
		}

		switch (code) {
		case 0:
			o.procState.currFG = TermColor::DefaultForeGround();
			o.procState.currBG = TermColor::DefaultBackGround();
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
		case 38:
		case 48: {
			bool isForeground = (code == 38);
			if (i + 1 < codes.size()) {
				int mode = std::stoi(codes[++i]);
				if (mode == 5 && i + 1 < codes.size()) {
					// 256-color mode
					int index = std::stoi(codes[++i]);
					TermColor col = colorFrom256(index);
					if (isForeground)
						o.procState.currFG = col;
					else
						o.procState.currBG = col;
				} else if (mode == 2 && i + 3 < codes.size()) {
					// Truecolor mode
					int r = std::stoi(codes[++i]);
					int g = std::stoi(codes[++i]);
					int b = std::stoi(codes[++i]);
					TermColor col(r, g, b);
					if (isForeground)
						o.procState.currFG = col;
					else
						o.procState.currBG = col;
				}
			}
			continue;
		}
		}

		int idx = -1;
		bool isForeGround = false;
		if (code >= 30 && code <= 37) {
			idx = code - 30;
			isForeGround = true;
		} else if (code >= 90 && code <= 97) {
			idx = code - 90 + 8;
			isForeGround = true;
		} else if (code >= 40 && code <= 47) {
			idx = code - 40;
			isForeGround = false;
		} else if (code >= 100 && code <= 107) {
			idx = code - 100 + 8;
			isForeGround = false;
		}
		if (idx != -1) {
			TermColor& target = isForeGround ? o.procState.currFG : o.procState.currBG;
			if (idx >= 0 && idx < 16) {
				target = kBasicColors[idx];
			} else {
				if (isForeGround) {
					target = TermColor::DefaultForeGround();
				} else {
					target = TermColor::DefaultBackGround();
				}
			}
		} else {
			std::cout << "INVALID COLOR\n";
		}
	}
}

void handleGraphicMode(std::string_view data, bool enable) {
	int mode = 0;
	try {
		mode = std::stoi(data);
	} catch (...) {
		return; // Invalid mode, ignore
	}

	std::cout << "GRAPHIC MODE: " << mode << " " << (enable ? "ENABLE" : "DISABLE") << "\n";
	switch (mode) {
	case 0: {
		// 40 x 25 monochrome (text)
		break;
	}
	case 1: {
		// 40 x 25 color (text)
		break;
	}
	case 2: {
		// 80 x 25 monochrome (text)
		break;
	}
	case 3: {
		// 80 x 25 color (text)
		break;
	}
	case 4: {
		// 320 x 200 4-color (graphics)
		break;
	}
	case 5: {
		// 320 x 200 monochrome (graphics)
		break;
	}
	case 6: {
		// 640 x 200 monochrome (graphics)
		break;
	}
	case 7: {
		// Enables line wrapping
		setFlag(TermFlags::OUTPUT_WRAP_LINES, enable);
		break;
	}
	case 13: {
		// 320 x 200 color (graphics)
		break;
	}
	case 14: {
		// 640 x 200 color (16-color graphics)
		break;
	}
	case 15: {
		// 640 x 350 monochrome (2-color graphics)
		break;
	}
	case 16: {
		// 640 x 350 color (16-color graphics)
		break;
	}
	case 17: {
		// 640 x 480 monochrome (2-color graphics)
		break;
	}
	case 18: {
		// 640 x 480 color (16-color graphics)
		break;
	}
	case 19: {
		// 320 x 200 color (256-color graphics)
		break;
	}
	default: {
		// Unknown or unsupported graphic mode
		break;
	}
	}
}

void handleDECPrivateMode(std::string_view data, bool enable) {
	// Handles DEC Private Mode Set/Reset sequences (e.g., ESC[?7h, ESC[?25l)
	int mode = 0;
	try {
		mode = std::stoi(data);
	} catch (...) {
		return; // Invalid mode, ignore
	}

	switch (mode) {
	case 1:
		// Input: Map LF to CRLF
		setFlag(TermFlags::INPUT_LF_TO_CRLF, enable);
		break;
	case 2:
		// Input: Echo input characters
		setFlag(TermFlags::INPUT_ECHO, enable);
		break;
	case 7:
		// Output: Auto-wrap lines
		setFlag(TermFlags::OUTPUT_WRAP_LINES, enable);
		break;
	case 25:
		// Cursor: Show or hide cursor
		o.showCursor = enable;
		break;
	case 1004:
		// Focus: Track focus events
		setFlag(TermFlags::TRACK_FOCUS, enable);
		break;
	case 1049:
		// Alternate screen buffer (not implemented)
		break;
	case 1047:
		// Alternate screen buffer (not implemented)
		break;
	case 1048:
		// Save/restore cursor (not implemented)
		break;
	case 9001:
		// Wrap pasted text in ESC[200~ and ESC[201~ sequences
		setFlag(TermFlags::BRACKETED_PASTE, enable);
		break;
	default:
		// Unknown or unsupported mode
		break;
	}
}

void handleEraseInDisplay(int mode) {
	switch (mode) {
	case 0: { // Erase from cursor to end of screen
		for (int y = o.cursorY; y < o.rows; ++y) {
			StyledLine line = o.screen[y];
			int start = (y == o.cursorY) ? o.cursorX : 0;
			for (size_t x = start; x < line.size(); ++x) {
				line[x] = makeStyledChar(U' ');
			}
		}
		break;
	}
	case 1: { // Erase from start to cursor
		for (int y = 0; y <= o.cursorY; ++y) {
			StyledLine line = o.screen[y];
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
		break;
	}
	case 3: { // Erase scrollback buffer
		o.screen.clearScrollback();
		break;
	}
	default:
		// Unknown mode, ignore
		break;
	}
}

void handleCSI() {
	std::string& csiData = o.procState.escBuf;
	char type = csiData.back();
	csiData.pop_back();

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
		int moveUpBy = std::stoi(csiData, 1);
		if (o.cursorY > moveUpBy) {
			o.cursorY -= moveUpBy;
		} else {
			o.cursorY = 0;
		}
		break;
	}
	case 'B': {
		int moveDownBy = std::stoi(csiData, 1);
		o.cursorY += moveDownBy;
		break;
	}
	case 'C': {
		int moveForwardBy = std::stoi(csiData, 1);
		o.cursorX += moveForwardBy;
		break;
	}
	case 'D': {
		int moveBackwardsBy = std::stoi(csiData, 1);
		o.cursorX -= moveBackwardsBy;
		break;
	}
	case 'I': {
		int count = std::stoi(csiData, 1);
		StyledChar blankChar = makeStyledChar(U' ');
		StyledLine line = o.screen[o.cursorY];
		for (int i = o.cursorX; i < line.size() && count > 0; ++i, --count) {
			line[i] = blankChar;
		}
		break;
	}
	case 'l':
	case 'h': {
		if (csiData.empty()) {
			break;
		}
		std::string_view data = std::string_view(csiData.data() + 1, csiData.size() - 1);
		if (csiData[0] == '?') {
			handleDECPrivateMode(data, type == 'h');
		} else if (csiData[0] == '=') {
			// handles changing the graphic mode
			handleGraphicMode(data, type == 'h');
		}
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
			int row = (params.size() > 0 && !params[0].empty()) ? std::stoi(params[0]) : 1;
			int col = (params.size() > 1 && !params[1].empty()) ? std::stoi(params[1]) : 1;
			o.cursorY = std::max(0, row - 1);
			o.cursorX = std::max(0, col - 1);
		} catch (...) {
		}
		break;
	}
	case 'K': {
		int mode = 0;
		try {
			if (csiData.empty())
				mode = 0;
			else {
				mode = std::stoi(csiData);
			}
		} catch (...) {
		}
		if (mode == 0) {
			StyledLine line = o.screen[o.cursorY];
			if (!line.empty()) {
				for (size_t i = o.cursorX; i < line.size(); ++i) {
					line[i] = makeStyledChar(U' ');
				}
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
	case 'X': {
		int numOfSpace = std::stoi(csiData);
		StyledLine line = o.screen[o.cursorY];
		for (int i = 0; i < numOfSpace && o.cursorX + i < line.size(); ++i) {
			line[o.cursorX + i] = makeStyledChar(U' ');
		}
		break;
	}
	default: {
		std::cout << "[" << type << "]'" << csiData << "'\n";
		break;
	}
	}

	csiData.clear();
}

void handleOSC() {
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
}

void processPartialOutputSegment(const std::vector<char>& inputSegment) {
	o.procState.leftover.append(inputSegment.data(), inputSegment.size());
	size_t i = 0;
	std::vector<char> utf8Accum;

	while (i < o.procState.leftover.size()) {
		char c = o.procState.leftover[i];

		switch (o.procState.state) {
		case ProcState::None:
			switch (c) {
			case '\033': { // ESC
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
				i++;
				break;

			case '\t': // Tab
				for (int t = 0; t < 4; ++t) {
					o.screen.atCursor() = makeStyledChar(U' ');
					o.cursorX++;
				}
				i++;
				break;

			case '\b': { // Backspace
				if (o.cursorX > 0) {
					o.cursorX--;
				}
				i++;
				break;
			}
			case '\n': {
				// Commit the current line and reset
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
				const char* utf8Buf = utf8Accum.data();
				const char* utf8End = utf8Buf + utf8Accum.size();
				while (utf8Buf < utf8End) {
					uint32_t cp;
					int len = decode_utf8(utf8Buf, &cp);
					if (len <= 0 || utf8Buf + len > utf8End)
						break;
					utf8Buf += len;

					o.screen.atCursor() = makeStyledChar(cp);
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
				// Lone CR
				o.cursorX = 0;
				// Do not increment i, reprocess the current character
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
}