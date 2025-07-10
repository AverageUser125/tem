#pragma once

#include <string>
#include <vector>
#include <string_view>

void startRender(float fontSize);
void render(const std::vector<std::string>& lines, int startLineIndex, int screenW, int screenH);
void renderCursor(int cursorX, int cursorY, float deltaTime, int screenW, int screenH);
void stopRender();