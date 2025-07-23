#include <cstdint>
#include "main.h"
#include <string>
#include <platform/input.h>
#include <platform/window.h>
#include "utf8.h"

void processInput() {
	const std::u32string& typed = platform::getTypedInput();
	for (uint32_t ch : typed) {
		if (ch < 32)
			continue;
		char buf[4]{};
		int bytesWritten = encode_utf8(ch, buf);
		o.command.append(buf, bytesWritten);
	}

	if (platform::isButtonTyped(platform::Button::Backspace)) {
		o.command.append("\x7F");
	}
	if (platform::isButtonTyped(platform::Button::Left)) {
		o.command.append("\x1b[D"); // Move cursor left
	}
	if (platform::isButtonTyped(platform::Button::Right)) {
		o.command.append("\x1b[C"); // Move cursor right
	}
	if (platform::isButtonTyped(platform::Button::Up)) {
		o.command.append("\x1b[A"); // Move cursor up
	}
	if (platform::isButtonTyped(platform::Button::Down)) {
		o.command.append("\x1b[B"); // Move cursor down
	}

	if ((platform::isButtonHeld(platform::Button::LeftCtrl) || platform::isButtonPressed(platform::Button::LeftCtrl)) &&
		platform::isButtonPressed(platform::Button::V)) {
		const char* clip = platform::getClipboard(); // null-terminated UTF-8
		if (clip) {
			if (o.flags.has(TermFlags::BRACKETED_PASTE)) {
				o.command += "\x1b[200~"; // Start bracketed paste
				o.command += clip;
				o.command += "\x1b[201~"; // End bracketed paste
			} else {
				o.command += clip; // Just append to command
			}
		}
	}

	if (platform::isButtonTyped(platform::Button::Enter)) {
		o.command += '\n';
	}

	if (!o.command.empty()) {
		o.scrollbackOffset = 0;
	}

	if (o.flags.has(TermFlags::INPUT_LF_TO_CRLF)) {
		size_t pos = 0;
		while ((pos = o.command.find('\n', pos)) != std::string::npos) {
			o.command.replace(pos, 1, "\r\n");
			pos += 2;
		}
	}

	if (o.flags.has(TermFlags::TRACK_FOCUS)) {
		static bool wasFocused = platform::hasFocused();
		bool isFocused = platform::hasFocused();
		if (isFocused != wasFocused) {
			wasFocused = isFocused;
			std::string_view focusCode = isFocused ? "\033[I" : "\033[O";
			o.command.append(focusCode.data(), focusCode.size());
		}
	}
}
