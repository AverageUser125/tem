#pragma once

#include <string>
#include <vector>
#include <string_view>

void startRender(float fontSize);
void render(const std::vector<std::string>& lines, int startLineIndex, float fontSize, int screenW, int screenH);
