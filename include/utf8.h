#pragma once
#include <cstdint>
#include <string_view>
int decode_utf8(const char* s, char32_t* codepoint);
int encode_utf8(char32_t codepoint, char* out);
int get_length(std::string_view sv);
int codepoint_length(const char* s);