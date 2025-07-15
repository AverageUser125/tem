#include <platform/shell.h>
#include <vector>
#include <thread>
#include <string_view>
#include <platform/tools.h>
#include <stdexcept>
#include <stdio.h>

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
#include <pty.h>
#endif

namespace platform
{
#ifdef _WIN32
class WinProcess : public Process {
	HPCON hPC = nullptr;
	HANDLE hInputWrite = nullptr;
	HANDLE hOutputRead = nullptr;
	HANDLE hProcess = nullptr;
	std::vector<char> buffer;

  public:
	WinProcess(std::string_view cmd) {
		// Create pipes
		HANDLE hInputRead = nullptr;
		HANDLE hOutputWrite = nullptr;
		SECURITY_ATTRIBUTES sa{sizeof(sa), nullptr, TRUE};

		if (!CreatePipe(&hInputRead, &hInputWrite, &sa, 0))
			throw std::runtime_error("CreatePipe (in) failed");
		if (!CreatePipe(&hOutputRead, &hOutputWrite, &sa, 0))
			throw std::runtime_error("CreatePipe (out) failed");

		// Create the pseudo console
		COORD size{80, 25};
		HRESULT hr = CreatePseudoConsole(size, hInputRead, hOutputWrite, 0, &hPC);
		CloseHandle(hInputRead);
		CloseHandle(hOutputWrite);
		if (FAILED(hr))
			throw std::runtime_error("CreatePseudoConsole failed");

		// Setup attribute list
		STARTUPINFOEXA si{};
		si.StartupInfo.cb = sizeof(si);
		SIZE_T attrListSize = 0;
		InitializeProcThreadAttributeList(nullptr, 1, 0, &attrListSize);
		si.lpAttributeList = (LPPROC_THREAD_ATTRIBUTE_LIST)HeapAlloc(GetProcessHeap(), 0, attrListSize);
		InitializeProcThreadAttributeList(si.lpAttributeList, 1, 0, &attrListSize);
		UpdateProcThreadAttribute(si.lpAttributeList, 0, PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE, hPC, sizeof(hPC), nullptr,
								  nullptr);

		// Start process
		PROCESS_INFORMATION pi{};
		std::string command = std::string(cmd);
		BOOL ok = CreateProcessA(nullptr, command.data(), nullptr, nullptr, FALSE, EXTENDED_STARTUPINFO_PRESENT,
								 nullptr, nullptr, &si.StartupInfo, &pi);

		DeleteProcThreadAttributeList(si.lpAttributeList);
		HeapFree(GetProcessHeap(), 0, si.lpAttributeList);

		if (!ok)
			throw std::runtime_error("CreateProcess failed");

		CloseHandle(pi.hThread);
		hProcess = pi.hProcess;
		buffer.reserve(4096);
	}

	void write(const char* data, size_t len) override {
		DWORD written = 0;
		WriteFile(hInputWrite, data, (DWORD)len, &written, nullptr);
	}

	void update() override {
		DWORD available = 0;
		if (!PeekNamedPipe(hOutputRead, nullptr, 0, nullptr, &available, nullptr))
			return;
		if (available == 0)
			return;

		size_t oldSize = buffer.size();
		buffer.resize(oldSize + available);
		DWORD read = 0;
		if (ReadFile(hOutputRead, buffer.data() + oldSize, available, &read, nullptr)) {
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
		DWORD code = 0;
		GetExitCodeProcess(hProcess, &code);
		return code == STILL_ACTIVE;
	}

	void terminate() override {
		if (hProcess)
			TerminateProcess(hProcess, 0);
	}

	~WinProcess() override {
		terminate();
		if (hPC)
			ClosePseudoConsole(hPC);
		if (hInputWrite)
			CloseHandle(hInputWrite);
		if (hOutputRead)
			CloseHandle(hOutputRead);
		if (hProcess)
			CloseHandle(hProcess);
	}
};

#else

class PosixProcess : public Process {
	std::vector<char> buffer;
	int pid = -1;
	int masterFD = -1;

  public:
	PosixProcess(std::string_view cmd) {
		struct winsize ws = {24, 80, 0, 0}; // fake terminal size

		pid = forkpty(&masterFD, nullptr, nullptr, &ws);
		permaAssertComment(pid != -1, "forkpty() failed");

		if (pid == 0) {
			// Child process
			execl(cmd.data(), cmd.data(), (char*)nullptr);
			_exit(127);
		}

		// Parent process
		int flags = fcntl(masterFD, F_GETFL, 0);
		permaAssertComment(flags != -1, "fcntl(F_GETFL) failed");
		permaAssertComment(fcntl(masterFD, F_SETFL, flags | O_NONBLOCK) != -1, "fcntl(F_SETFL) failed");

		buffer.reserve(4096);

		int status;
		pid_t result = waitpid(pid, &status, WNOHANG);
		permaAssertComment(result == 0, "Process exited early");
	}

	void write(const char* data, size_t len) override {
		::write(masterFD, data, len);
	}

	void update() override {
		char temp[1024];
		ssize_t count = ::read(masterFD, temp, sizeof(temp));
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
		if (masterFD != -1)
			close(masterFD);
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

}
