#include <platform/shell.h>
#include <vector>
#include <thread>
#include <string_view>
#include <platform/tools.h>
#include <stdexcept>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#else
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <csignal>
#include <cstring>
#endif

namespace platform
{

#ifdef _WIN32

class WinProcess : public Process {
	std::vector<char> buffer;
	HANDLE hChildStdinWrite = nullptr;
	HANDLE hChildStdoutRead = nullptr;
	HANDLE hProcess = nullptr;

  public:
	WinProcess(std::string_view cmd) {
		SECURITY_ATTRIBUTES sa = {sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
		HANDLE hStdoutReadTmp = nullptr, hStdoutWrite = nullptr;
		HANDLE hStdinRead = nullptr, hStdinWriteTmp = nullptr;

		CreatePipe(&hStdoutReadTmp, &hStdoutWrite, &sa, 0);
		SetHandleInformation(hStdoutReadTmp, HANDLE_FLAG_INHERIT, 0);

		CreatePipe(&hStdinRead, &hStdinWriteTmp, &sa, 0);
		SetHandleInformation(hStdinWriteTmp, HANDLE_FLAG_INHERIT, 0);

		STARTUPINFOA si = {};
		si.cb = sizeof(si);
		si.dwFlags = STARTF_USESTDHANDLES;
		si.hStdOutput = hStdoutWrite;
		si.hStdError = hStdoutWrite;
		si.hStdInput = hStdinRead;

		PROCESS_INFORMATION pi;
		BOOL success = CreateProcessA(nullptr, const_cast<char*>(cmd.data()), nullptr, nullptr, TRUE, CREATE_NO_WINDOW,
									  nullptr, nullptr, &si, &pi);

		CloseHandle(hStdoutWrite);
		CloseHandle(hStdinRead);
		if (!success)
			throw std::runtime_error("CreateProcess failed");

		hChildStdinWrite = hStdinWriteTmp;
		hChildStdoutRead = hStdoutReadTmp;
		hProcess = pi.hProcess;
		CloseHandle(pi.hThread);

		buffer.reserve(4096);
	}

	void write(const char* data, size_t len) override {
		DWORD written;
		WriteFile(hChildStdinWrite, data, (DWORD)len, &written, nullptr);
	}

	void update() override {
		DWORD available = 0;
		if (!PeekNamedPipe(hChildStdoutRead, nullptr, 0, nullptr, &available, nullptr))
			return;
		if (available == 0)
			return;

		size_t oldSize = buffer.size();
		buffer.resize(oldSize + available);
		DWORD read = 0;
		if (ReadFile(hChildStdoutRead, buffer.data() + oldSize, available, &read, nullptr)) {
			buffer.resize(oldSize + read);
		} else {
			buffer.resize(oldSize); // rollback
		}
	}

	std::vector<char>& getOutputBuffer() override {
		return buffer;
	}

	bool isRunning() const override {
		if (!hProcess)
			return false;
		DWORD exitCode;
		GetExitCodeProcess(hProcess, &exitCode);
		return exitCode == STILL_ACTIVE;
	}

	void terminate() override {
		if (hProcess)
			TerminateProcess(hProcess, 0);
	}

	~WinProcess() override {
		terminate();
		CloseHandle(hChildStdoutRead);
		CloseHandle(hChildStdinWrite);
		CloseHandle(hProcess);
	}
};

#else

class PosixProcess : public Process {
	std::vector<char> buffer;
	int pid = -1;
	int writeFD = -1;
	int readFD = -1;

  public:
	PosixProcess(std::string_view cmd) {
		int pipeIn[2], pipeOut[2];

		permaAssertComment(pipe(pipeIn) != -1, "pipe(pipeIn) failed");
		permaAssertComment(pipe(pipeOut) != -1, "pipe(pipeOut) failed");

		pid = fork();
		permaAssertComment(pid != -1, "fork() failed");

		if (pid == 0) {
			// Child process
			dup2(pipeIn[0], STDIN_FILENO);
			dup2(pipeOut[1], STDOUT_FILENO);
			dup2(pipeOut[1], STDERR_FILENO);
			close(pipeIn[1]);
			close(pipeOut[0]);

			// Try to exec the given command
			execl(cmd.data(), cmd.data(), (char*)nullptr);

			// If we reach here, execl failed
			_exit(127); // Indicate exec failure
		}

		// Parent process
		close(pipeIn[0]);
		close(pipeOut[1]);
		writeFD = pipeIn[1];
		readFD = pipeOut[0];
		buffer.reserve(4096);

		// Make readFD non-blocking
		int flags = fcntl(readFD, F_GETFL, 0);
		permaAssertComment(flags != -1, "fcntl(F_GETFL) failed");
		permaAssertComment(fcntl(readFD, F_SETFL, flags | O_NONBLOCK) != -1, "fcntl(F_SETFL) failed");

		// Check if child exited immediately (exec failure)
		int status;
		pid_t result = waitpid(pid, &status, WNOHANG);
		permaAssertComment(result == 0, "Process exited early (exec failed or crashed)");
	}

	void write(const char* data, size_t len) override {
		::write(writeFD, data, len);
	}

	void update() override {
		char temp[1024];
		ssize_t count = ::read(readFD, temp, sizeof(temp));
		if (count > 0) {
			buffer.insert(buffer.end(), temp, temp + count);
		}
	}

	std::vector<char>& getOutputBuffer() override {
		return buffer;
	}

	bool isRunning() const override {
		if (pid == -1)
			return false;
		int status;
		return waitpid(pid, &status, WNOHANG) == 0;
	}

	void terminate() override {
		if (pid != -1)
			kill(pid, SIGTERM);
	}

	~PosixProcess() override {
		terminate();
		if (writeFD != -1)
			close(writeFD);
		if (readFD != -1)
			close(readFD);
	}
};

#endif

Process* launch() {
#ifdef _WIN32
	return new WinProcess("cmd.exe /Q /K");
#else
	return new PosixProcess("/bin/bash");
#endif
}

} // namespace
