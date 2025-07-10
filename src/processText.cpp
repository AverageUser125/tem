#include "main.h"
#include "processText.h"
#include "utf8.h"
#include "bitflags.hpp"

struct ProcState {
  public:
	enum Value {
		None = 0,
		SawESC = 1 << 0,
		SawESCBracket = 1 << 1,
		SawCR = 1 << 2,
	};

	private:
	DEFINE_BITFLAGS(ProcState);
};

struct InputProcessorState {
	std::string leftover;
	ProcState state = ProcState::None;
};

static InputProcessorState processorState;

std::vector<char> processPartialInputSegment(const std::vector<char>& inputSegment) {
	processorState.leftover.insert(processorState.leftover.end(), inputSegment.begin(), inputSegment.end());

	std::vector<char> output;
	size_t i = 0;

	while (i < processorState.leftover.size()) {
		char c = processorState.leftover[i];

		if ((int)processorState.state == (int)ProcState::None) {
			if (c == '\033') {
				processorState.state.set(ProcState::SawESC);
				i++;
			} else if (c == '\r') {
				processorState.state.set(ProcState::SawCR);
				i++;
			} else if (c == '\f') {
				o.lines.clear();
				o.cursorX = 0;
				o.cursorY = 0;
				i++;
			} else {
				output.push_back(c);
				i++;
			}
		} else if (processorState.state.has(ProcState::SawCR)) {
			if (c == '\n') {
				output.push_back('\n'); // CRLF -> LF
				processorState.state.reset();
				i++;
			} else {
				output.push_back('\r'); // Lone CR
				processorState.state.reset();
				// o.cursorX = 0; defintion of carriage
				// Reprocess current char
			}
		} else if (processorState.state.has(ProcState::SawESC)) {
			if (c == '[') {
				processorState.state.set(ProcState::SawESCBracket);
				processorState.state.clear(ProcState::SawESC);
				i++;
			} else {
				processorState.state.reset();
				i++;
			}
		} else if (processorState.state.has(ProcState::SawESCBracket)) {
			if ((unsigned char)c >= 0x40 && (unsigned char)c <= 0x7E) {
				processorState.state.reset(); // End escape seq
				i++;
			} else {
				i++; // Skip until terminator
			}
		} else {
			processorState.state.reset();
			i++;
		}

		if (i == processorState.leftover.size() && processorState.state.has(ProcState::SawESCBracket)) {
			break; // Leave partial escape in leftover
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
