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
			code = std::stoi(std::string(part));
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

	// Extract mode number substring after '?'
	std::string_view modeStr = params.substr(1);

	int mode = 0;
	try {
		mode = std::stoi(std::string(modeStr));
	} catch (...) {
		return; // invalid number, ignore
	}

	// Map mode number to TermFlag (reuse your existing function or inline)
	TermFlags::Value flag = termFlagFromNumber(mode);

	if (flag == TermFlags::NONE) {
		return; // nothing to do for unknown modes
	}

	if (finalChar == 'h') {
		// Set the flag
		o.flags |= flag;
	} else if (finalChar == 'l') {
		// Clear the flag
		o.flags &= ~flag;
	}
}

static void handleCSI() {
	std::string& csiData = o.procState.escBuf;
	char type = csiData.back();
	csiData.pop_back();
	
	switch (type) {
	case 'm': {
		applySGRColor(csiData);
		break;
	}
	case 'G':
	{
		int col = std::stoi(csiData);
		o.cursorX = col;
		break;
	}
	case 'A':
	{
		// TODO: more protections
		int moveUpBy = std::stoi(csiData);
		if (o.cursorY > moveUpBy) {
			o.cursorY -= moveUpBy;		
		} else {
			o.cursorY = 0;
		}
		break;
	}
	case 'B':
	{
		// TODO: more protections
		int moveDownBy = std::stoi(csiData);
		o.cursorY += moveDownBy;
		break;
	}
	case 'C':
	{
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
	case 'I':
	{
		int count = 1;
		try {
			count = std::stoi(std::string(csiData));
		} catch (...) {
			count = 1;
		}
		StyledChar blankChar;
		blankChar.ch = U' '; // space
		blankChar.fg = o.procState.currFG;
		blankChar.bg = o.procState.currBG;
		blankChar.attr = o.procState.currAttr;
		StyledLine& line = o.screen[o.cursorY];
		line.insert(line.begin() + o.cursorX, count, blankChar);

		break;
	}
	case 'l': 
	case 'h': {
		handleDECPrivateMode(csiData, type);
		break;
	}
	default:
	{
		std::cout << "[" << type << "]'" << csiData << "'\n";
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

	std::string_view param = oscData.substr(0, semicolonPos);
	std::string_view content = oscData.substr(semicolonPos + 1);

	int paramNum = 0;
	try {
		paramNum = std::stoi(std::string(param));
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

std::vector<char> processPartialOutputSegment(const std::vector<char>& inputSegment) {
	o.procState.leftover.insert(o.procState.leftover.end(), inputSegment.begin(), inputSegment.end());

	std::vector<char> output;
	size_t i = 0;

	while (i < o.procState.leftover.size()) {
		char c = o.procState.leftover[i];

		switch (o.procState.state) {
		case ProcState::None:
			switch (c) {
			case '\033': // ESC
				o.procState.state = ProcState::SawESC;
				i++;
				break;

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
				output.insert(output.end(), {' ', ' ', ' ', ' '});
				i++;
				break;

			case '\b': // Backspace
				o.cursorX--;
				break;

			default:
				output.push_back(c);
				i++;
				break;
			}
			break;

		case ProcState::SawCR:
			if (c == '\n') {
				output.push_back('\n'); // CRLF -> LF
				i++;
			} else {
				output.push_back('\r'); // Lone CR
										// reprocess current character
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

	return output;
}

void appendNewLines(const std::vector<char>& buf) {
	if (buf.empty())
		return;

	const char* bufData = buf.data();
	const char* bufEnd = bufData + buf.size();
	const char* iter = bufData;

	StyledLine currentLine;

	while (iter < bufEnd) {
		uint32_t cp;
		int len = decode_utf8(iter, &cp);
		if (len <= 0 || iter + len > bufEnd)
			break;

		iter += len;

		if (cp == '\n') {
			// Commit the current line and reset
			o.screen.push_back(std::move(currentLine));
			currentLine.clear();
			o.cursorX = 0;
			o.cursorY++;
		} else {
			// Append character with current style
			currentLine.push_back(
				StyledChar{cp, o.procState.currFG, o.procState.currBG, o.procState.currAttr});
		}
	}

	// Save partial line (if any)
	if (!currentLine.empty()) {
		o.screen.push_back(std::move(currentLine));
		o.cursorX = int(o.screen.back().size());
		o.cursorY = int(o.screen.size() - 1);
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
