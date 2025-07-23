#pragma once

#include <string_view>
#include <cstddef>
#include <vector>

namespace platform
{

class Process {
	std::vector<char> buffer;
#ifdef _WIN32
	using W_HPCON = void*;
	using W_HANDLE = void*;
	W_HPCON hPC = nullptr;
	W_HANDLE hInputWrite = nullptr;
	W_HANDLE hOutputRead = nullptr;
	W_HANDLE hProcess = nullptr;
#else
	int pid = -1;
	int masterFd = -1;
#endif
  public:
	Process() = default;
	~Process();
	void launch();
	void write(const char* data, size_t len);
	// Call periodically to pump in new data from the process
	void update();
	// The buffer is updated by `update()`
	std::vector<char>& getOutputBuffer();
	bool isRunning() const;
	void terminate();
	void resize(int collumns, int rows);
};
}