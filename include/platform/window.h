#pragma once
namespace platform
{
///sets the mouse pos relative to the window's drawing area
void setRelMousePosition(int x, int y);

bool isFullScreen();
void setFullScreen(bool f);

//gets the window size, can be different to the actual framebuffer size
//you should use getFrameBufferSize if you want the actual ammount of pixels to give to your opengl routines
void getWindowSize(int* x, int *y);

//usually is the same as getWindowSize unless you have things like zoom or rezolution.
//You should use this function if you want to pass this data to glviewport
void getFrameBufferSize(int* x, int* y);

///gets the mouse pos relative to the window's drawing area
void getRelMousePosition(int* x, int* y);

void showMouse(bool show);
bool hasFocused();
bool mouseMoved();

void setWindowTitle(const char* title);

}; // namespace platform