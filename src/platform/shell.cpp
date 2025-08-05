#include <platform/shell.h>
#include <vector>
#include <string_view>
#include <stdexcept>
#include <platform/tools.h>
#include <stdio.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

namespace platform
{

void Process::launch(int rows, int cols) {
	std::string_view cmd = "cmd.exe /Q /K";
	HANDLE hInputRead = nullptr;
	HANDLE hOutputWrite = nullptr;
	SECURITY_ATTRIBUTES sa{sizeof(sa), nullptr, TRUE};

	permaAssertComment(CreatePipe(&hInputRead, &hInputWrite, &sa, 0), "CreatePipe (in) failed");
	permaAssertComment(CreatePipe(&hOutputRead, &hOutputWrite, &sa, 0), "CreatePipe (out) failed");

	COORD size{(SHORT)rows, (SHORT)cols};
	HRESULT hr = CreatePseudoConsole(size, hInputRead, hOutputWrite, 0, &hPC);
	CloseHandle(hInputRead);
	CloseHandle(hOutputWrite);
	permaAssertComment(!FAILED(hr), "CreatePseudoConsole failed");

	STARTUPINFOEXA si{};
	si.StartupInfo.cb = sizeof(si);
	SIZE_T attrListSize = 0;
	InitializeProcThreadAttributeList(nullptr, 1, 0, &attrListSize);
	si.lpAttributeList = (LPPROC_THREAD_ATTRIBUTE_LIST)HeapAlloc(GetProcessHeap(), 0, attrListSize);
	InitializeProcThreadAttributeList(si.lpAttributeList, 1, 0, &attrListSize);
	UpdateProcThreadAttribute(si.lpAttributeList, 0, PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE, hPC, sizeof(hPC), nullptr,
							  nullptr);

	PROCESS_INFORMATION pi{};
	std::string command = std::string(cmd);
	BOOL ok = CreateProcessA(nullptr, command.data(), nullptr, nullptr, FALSE, EXTENDED_STARTUPINFO_PRESENT, nullptr,
							 nullptr, &si.StartupInfo, &pi);

	DeleteProcThreadAttributeList(si.lpAttributeList);
	HeapFree(GetProcessHeap(), 0, si.lpAttributeList);

	permaAssertComment(ok, "CreateProcess failed");

	CloseHandle(pi.hThread);
	hProcess = pi.hProcess;
	buffer.reserve(4096);
}

Process::~Process() {
	terminate();
}

void Process::write(const char* data, size_t len) {
	DWORD written = 0;
	WriteFile(hInputWrite, data, (DWORD)len, &written, nullptr);
}

void Process::update() {
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

std::vector<char>& Process::getOutputBuffer() {
	return buffer;
}

bool Process::isRunning() const {
	if (!hProcess)
		return false;
	DWORD code = 0;
	GetExitCodeProcess(hProcess, &code);
	return code == STILL_ACTIVE;
}

void Process::terminate() {
	if (hProcess)
		TerminateProcess(hProcess, 0);
	if (hPC)
		ClosePseudoConsole(hPC);
	if (hInputWrite)
		CloseHandle(hInputWrite);
	if (hOutputRead)
		CloseHandle(hOutputRead);
	if (hProcess)
		CloseHandle(hProcess);
	hProcess = nullptr;
	hPC = nullptr;
	hInputWrite = nullptr;
	hOutputRead = nullptr;
}

void Process::resize(int columns, int rows) {
	COORD size;
	size.X = static_cast<SHORT>(columns);
	size.Y = static_cast<SHORT>(rows);
	HRESULT hr = ResizePseudoConsole(hPC, size);
	permaAssertComment(!FAILED(hr), "ResizePseudoConsole failed");
}
} // namespace platform

#else // POSIX

#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <csignal>
#include <cstring>
#include <pty.h>

namespace platform
{
void Process::launch(int rows, int cols) {
	std::string_view cmd = "/bin/bash";
	struct winsize ws = {cols, rows - 1, 0, 0}; // fake terminal size
	pid = forkpty(&masterFd, nullptr, nullptr, &ws);
	permaAssertComment(pid != -1, "forkpty() failed");

	if (pid == 0) {
		execl(cmd.data(), cmd.data(), (char*)nullptr);
		_exit(127);
	}

	int flags = fcntl(masterFd, F_GETFL, 0);
	permaAssertComment(flags != -1, "fcntl(F_GETFL) failed");
	permaAssertComment(fcntl(masterFd, F_SETFL, flags | O_NONBLOCK) != -1, "fcntl(F_SETFL) failed");

	buffer.reserve(4096);

	int status;
	pid_t result = waitpid(pid, &status, WNOHANG);
	permaAssertComment(result == 0, "Process exited early");
}

Process::~Process() {
	terminate();
}

void Process::write(const char* data, size_t len) {
	::write(masterFd, data, len);
}

void Process::update() {
	char temp[1024];
	ssize_t count = ::read(masterFd, temp, sizeof(temp));
	if (count > 0) {
		buffer.insert(buffer.end(), temp, temp + count);
	}
}

std::vector<char>& Process::getOutputBuffer() {
	return buffer;
}

bool Process::isRunning() const {
	if (pid == -1)
		return false;
	int status;
	return waitpid(pid, &status, WNOHANG) == 0;
}

void Process::terminate() {
	if (pid != -1)
		kill(pid, SIGTERM);
	if (masterFd != -1)
		close(masterFd);
	masterFd = -1;
	pid = -1;
}

void Process::resize(int rows, int columns) {
	struct winsize ws{};
	ws.ws_col = static_cast<unsigned short>(columns);
	ws.ws_row = static_cast<unsigned short>(rows);
	ws.ws_xpixel = 0;
	ws.ws_ypixel = 0;
	int result = ioctl(masterFd, TIOCSWINSZ, &ws);
	permaAssertComment(result != -1, "ioctl(TIOCSWINSZ) failed");
	kill(pid, SIGWINCH);
}
} // namespace platform

#endif
