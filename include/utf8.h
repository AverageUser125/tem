#pragma once
#include <cstdint>
int decode_utf8(const char* s, uint32_t* codepoint);
int encode_utf8(uint32_t codepoint, char* out);