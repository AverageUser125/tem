#include "utf8.h"
int decode_utf8(const char* s, uint32_t* codepoint) {
	uint8_t c = static_cast<uint8_t>(s[0]);

	if (c < 0x80) {
		*codepoint = c;
		return 1;
	} else if (c < 0xC0) {
		return 0; // invalid: continuation byte can't start a character
	} else if (c < 0xE0) {
		if ((s[1] & 0xC0) != 0x80)
			return 0;
		*codepoint = ((s[0] & 0x1F) << 6) | (s[1] & 0x3F);
		return 2;
	} else if (c < 0xF0) {
		if ((s[1] & 0xC0) != 0x80 || (s[2] & 0xC0) != 0x80)
			return 0;
		*codepoint = ((s[0] & 0x0F) << 12) | ((s[1] & 0x3F) << 6) | (s[2] & 0x3F);
		return 3;
	} else if (c < 0xF8) {
		if ((s[1] & 0xC0) != 0x80 || (s[2] & 0xC0) != 0x80 || (s[3] & 0xC0) != 0x80)
			return 0;
		*codepoint = ((s[0] & 0x07) << 18) | ((s[1] & 0x3F) << 12) | ((s[2] & 0x3F) << 6) | (s[3] & 0x3F);
		return 4;
	}
	return 0; // invalid: > 4-byte UTF-8 (not supported)
}

int encode_utf8(uint32_t codepoint, char* out) {
	if (codepoint <= 0x7F) {
		out[0] = codepoint;
		out[1] = '\0';
		return 1;
	} else if (codepoint <= 0x7FF) {
		out[0] = 0xC0 | (codepoint >> 6);
		out[1] = 0x80 | (codepoint & 0x3F);
		out[2] = '\0';
		return 2;
	} else if (codepoint <= 0xFFFF) {
		out[0] = 0xE0 | (codepoint >> 12);
		out[1] = 0x80 | ((codepoint >> 6) & 0x3F);
		out[2] = 0x80 | (codepoint & 0x3F);
		out[3] = '\0';
		return 3;
	} else if (codepoint <= 0x10FFFF) {
		out[0] = 0xF0 | (codepoint >> 18);
		out[1] = 0x80 | ((codepoint >> 12) & 0x3F);
		out[2] = 0x80 | ((codepoint >> 6) & 0x3F);
		out[3] = 0x80 | (codepoint & 0x3F);
		out[4] = '\0';
		return 4;
	}

	// Invalid codepoint
	return 0;
}
