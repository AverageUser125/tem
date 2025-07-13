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

static void applySGRColor(std::string_view codeStr) {
	int code = 0;
	try {
		code = std::stoi(std::string(codeStr));
	} catch (...) {
		return; // Ignore invalid input
	}
	switch (code) {
	case 0:
		o.procState.currFG = TermColor::Default;
		o.procState.currBG = TermColor::Default;
		o.procState.currAttr = TextAttribute::None;
		return;
	case 1:
		o.procState.currAttr |= TextAttribute::Bold;
		return;
	case 3:
		o.procState.currAttr |= TextAttribute::Italic;
		return;
	case 4:
		o.procState.currAttr |=TextAttribute::Underline;
		return;
	case 7:
		o.procState.currAttr |= TextAttribute::Inverse;
		return;
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

static void handleEscapeCode() {
	char type = o.procState.escBuf.back();
	o.procState.escBuf.pop_back();

	switch (type) {
	case 'm': {
		applySGRColor(o.procState.escBuf);
		break;
	}
	default:
	{
		std::cout << "[" << type << "]'" << o.procState.escBuf << "'\n";
	}
	}

	o.procState.escBuf.clear();
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
				o.procState.state = ProcState::SawESCBracket;
			} else {
				o.procState.state = ProcState::None;
			}
			i++;
			break;

		case ProcState::SawESCBracket:
			o.procState.escBuf += c;
			if ((unsigned char)c >= 0x40 && (unsigned char)c <= 0x7E) {
				o.procState.state = ProcState::None;
				handleEscapeCode();
			}
			i++;
			break;
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
