#pragma once
#include "bitflags.hpp"
#include <string>
#include <vector>
#include <string_view>
#include <cstdint>
#include "main.h"

void startRender(float fontSize);
void render(StyledScreen& screen, int startLineIndex, int screenW, int screenH);
void renderCursor(int cursorX, int cursorY, float deltaTime, int screenW, int screenH);
void stopRender();