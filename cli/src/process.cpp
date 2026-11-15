#include <cstdio>
#include <filesystem>
#include <process.hpp>
#include <string>
#include <vector>
#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>
#endif
namespace kuki::cli {
namespace {
#ifdef _WIN32
  auto Widen(const std::string &text) -> std::wstring {
    if (text.empty())
      return {};
    const auto size = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    std::wstring wide(static_cast<size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), wide.data(), size);
    return wide;
  }
  /// One argument quoted the way the Microsoft C runtime splits a command line back into `argv`: in
  /// quotes when it has a space, a tab or a quote in it or is empty, a quote escaped with a
  /// backslash, and the backslashes before a quote -- including the closing one -- doubled.
  auto Quote(const std::wstring &argument) -> std::wstring {
    if (!argument.empty() && argument.find_first_of(L" \t\"") == std::wstring::npos)
      return argument;
    std::wstring quoted = L"\"";
    size_t backslashes = 0;
    for (const auto c : argument) {
      if (c == L'\\') {
        ++backslashes;
        continue;
      }
      if (c == L'"')
        quoted.append(backslashes * 2 + 1, L'\\');
      else
        quoted.append(backslashes, L'\\');
      backslashes = 0;
      quoted.push_back(c);
    }
    quoted.append(backslashes * 2, L'\\');
    quoted.push_back(L'"');
    return quoted;
  }
  auto Start(const std::vector<std::string> &arguments, const std::filesystem::path &directory, const bool quiet) -> int {
    std::wstring line;
    for (const auto &argument : arguments)
      line += (line.empty() ? L"" : L" ") + Quote(Widen(argument));
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    HANDLE null = INVALID_HANDLE_VALUE;
    if (quiet) {
      SECURITY_ATTRIBUTES inherit{.nLength = sizeof(SECURITY_ATTRIBUTES), .lpSecurityDescriptor = nullptr, .bInheritHandle = TRUE};
      null = CreateFileW(L"NUL", GENERIC_WRITE, FILE_SHARE_WRITE, &inherit, OPEN_EXISTING, 0, nullptr);
      startup.dwFlags = STARTF_USESTDHANDLES;
      startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
      startup.hStdOutput = null;
      startup.hStdError = null;
    }
    PROCESS_INFORMATION process{};
    // Searched for on the PATH, with `.exe` added, because the application name is left null.
    const auto started = CreateProcessW(nullptr, line.data(), nullptr, nullptr, TRUE, 0, nullptr, directory.empty() ? nullptr : directory.c_str(), &startup, &process);
    if (null != INVALID_HANDLE_VALUE)
      CloseHandle(null);
    if (!started)
      return -1;
    WaitForSingleObject(process.hProcess, INFINITE);
    DWORD code = 0;
    GetExitCodeProcess(process.hProcess, &code);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return static_cast<int>(code);
  }
#else
  auto Start(const std::vector<std::string> &arguments, const std::filesystem::path &directory, const bool quiet) -> int {
    const auto child = fork();
    if (child < 0)
      return -1;
    if (child == 0) {
      if (!directory.empty() && chdir(directory.c_str()) != 0)
        _exit(127);
      if (quiet) {
        const auto null = open("/dev/null", O_WRONLY);
        dup2(null, STDOUT_FILENO);
        dup2(null, STDERR_FILENO);
      }
      std::vector<char *> argv;
      for (const auto &argument : arguments)
        argv.push_back(const_cast<char *>(argument.c_str()));
      argv.push_back(nullptr);
      execvp(argv[0], argv.data());
      _exit(127);
    }
    int status = 0;
    waitpid(child, &status, 0);
    if (!WIFEXITED(status))
      return -1;
    // 127 is what the child exits with when the program could not be started, as a shell's is.
    return WEXITSTATUS(status) == 127 ? -1 : WEXITSTATUS(status);
  }
#endif
} // namespace
auto Run(const std::vector<std::string> &arguments, const std::filesystem::path &directory) -> int {
  const auto code = Start(arguments, directory, false);
  if (code == -1)
    std::fprintf(stderr, "kuki: could not run '%s'. Is it installed and on the PATH?\n", arguments.front().c_str());
  return code;
}
auto IsAvailable(const std::vector<std::string> &arguments) -> bool {
  return Start(arguments, {}, true) == 0;
}
} // namespace kuki::cli
