#pragma once

#include <string_view>
#include <cstddef>
#include <vector>

namespace platform
{

struct Process {
	virtual ~Process() = default;

	virtual void write(const char* data, size_t len) = 0;

	// Call periodically to pump in new data from the process
	virtual void update() = 0;

	// The buffer is updated by `update()`
	virtual std::vector<char>& getOutputBuffer() = 0;

	virtual bool isRunning() const = 0;

	virtual void terminate() = 0;
};

// cmd = shell command (e.g., "cmd.exe" or "bash")
// The returned Process* must be deleted by the user
Process* launch();

}
