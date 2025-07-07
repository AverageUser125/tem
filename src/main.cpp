#include "gameLogic.h"
#include <glad/glad.h>
#include <platform/window.h>
#include <platform/input.h>

bool gameLogic() {
	if (platform::isButtonPressed(platform::Button::F11)) {
		platform::setFullScreen(!platform::isFullScreen());
	}
	return true;
}

void closeGame() {
}

void startGame() {
}
