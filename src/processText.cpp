#include "main.h"
#include "processText.h"
#include "utf8.h"
#include "bitflags.hpp"
#include <string_view>

enum class ProcState : uint8_t {
	None,
	SawESC,
	SawESCBracket,
	SawCR,
};

struct InputProcessorState {
	std::string leftover;
	std::string escParamBuffer;
	ProcState state = ProcState::None;
};

static InputProcessorState processorState;

static void handleEscapeCode() {
	char type = processorState.escParamBuffer.pop_back();

	switch (type) {
	case 'm': {
		break;
	}
	case 'H': {
		break;
	}
	}

	processorState.escParamBuffer.clear();
}

std::vector<char> processPartialInputSegment(const std::vector<char>& inputSegment) {
	processorState.leftover.insert(processorState.leftover.end(), inputSegment.begin(), inputSegment.end());

	std::vector<char> output;
	size_t i = 0;

	while (i < processorState.leftover.size()) {
		char c = processorState.leftover[i];

		switch (processorState.state) {
		case ProcState::None:
			switch (c) {
			case '\033': // ESC
				processorState.state = ProcState::SawESC;
				i++;
				break;

			case '\r': // Carriage Return
				processorState.state = ProcState::SawCR;
				i++;
				break;

			case '\f': // Form Feed
				o.lines.clear();
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
			processorState.state = ProcState::None;
			break;

		case ProcState::SawESC:
			if (c == '[') {
				processorState.state = ProcState::SawESCBracket;
			} else {
				processorState.state = ProcState::None;
			}
			i++;
			break;

		case ProcState::SawESCBracket:
			processorState.escParamBuffer += c;
			if ((unsigned char)c >= 0x40 && (unsigned char)c <= 0x7E) {
				processorState.state = ProcState::None;
				handleEscapeCode();
			}
			i++;
			break;
		}
	}

	if (i > 0) {
		processorState.leftover.erase(processorState.leftover.begin(), processorState.leftover.begin() + i);
	}

	return output;
}

void appendNewLines(const std::vector<char>& buf) {
	if (buf.empty())
		return;
	const char* bufData = buf.data();
	const char* bufEnd = bufData + buf.size();
	const char* lineStart = bufData;
	const char* iter = bufData;

	while (iter < bufEnd) {
		uint32_t cp;
		int len = decode_utf8(iter, &cp);
		if (len <= 0 || iter + len > bufEnd)
			break;

		if (cp == '\n') {
			// Push the line up to but not including '\n'
			o.lines.emplace_back(lineStart, iter - lineStart);
			iter += len;
			lineStart = iter;

			o.cursorX = 0;
			o.cursorY++;
		} else {
			iter += len;
		}
	}

	// Handle any partial line at the end (no trailing '\n')
	if (lineStart < iter) {
		std::string partial(lineStart, iter - lineStart);
		if (!o.lines.empty() && !o.lines.back().empty()) {
			o.lines.back() += partial;
		} else {
			o.lines.emplace_back(std::move(partial));
		}
		o.cursorX = get_length(o.lines.back());
		o.cursorY = int(o.lines.size() - 1);
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
