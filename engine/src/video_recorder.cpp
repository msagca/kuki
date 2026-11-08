#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <optional>
#include <spdlog/spdlog.h>
#include <string>
#include <system_error>
#include <video_recorder.hpp>
#ifdef _WIN32
#include <windows.h>
#endif
namespace kuki {
namespace {
#ifdef _WIN32
  constexpr const wchar_t *ENCODER_NAME = L"ffmpeg.exe";
  constexpr wchar_t PATH_SEPARATOR = L';';
  using PathString = std::wstring;
  auto ReadPath() -> PathString {
    const auto length = GetEnvironmentVariableW(L"PATH", nullptr, 0);
    if (length == 0)
      return {};
    PathString value(length, L'\0');
    value.resize(GetEnvironmentVariableW(L"PATH", value.data(), length));
    return value;
  }
#else
  constexpr const char *ENCODER_NAME = "ffmpeg";
  constexpr char PATH_SEPARATOR = ':';
  using PathString = std::string;
  auto ReadPath() -> PathString {
    const auto *value = std::getenv("PATH");
    return value ? value : "";
  }
#endif
  /// What the encoder is told, after its own path and before the output's. Quiet but for errors, so
  /// a console build is not flooded with a progress line a frame; the size padded up to even, which
  /// the 4:2:0 a player can play needs and a window dragged to an odd width does not have; and the
  /// index at the front of the file, so the video starts playing in a browser before it is all there.
  /// A constant rate factor of 18 is about as good as the eye can tell, which is the point of a clip
  /// made to judge an effect by.
  auto EncoderArguments(const int width, const int height, const int rate) -> std::string {
    return std::format("-hide_banner -loglevel error -y -f rawvideo -pix_fmt rgba -s {}x{} -r {} -i - -vf pad=ceil(iw/2)*2:ceil(ih/2)*2 -c:v libx264 -preset medium -crf 18 -pix_fmt yuv420p -movflags +faststart", width, height, rate);
  }
} // namespace
VideoRecorder::~VideoRecorder() {
  if (IsOpen())
    Finish();
}
auto VideoRecorder::FindEncoder() -> std::optional<std::filesystem::path> {
  const auto path = ReadPath();
  for (size_t begin = 0; begin <= path.size();) {
    auto end = path.find(PATH_SEPARATOR, begin);
    if (end == PathString::npos)
      end = path.size();
    auto directory = path.substr(begin, end - begin);
    begin = end + 1;
    // A directory with a space in it is sometimes written quoted, which the shell understands and a
    // path does not.
    if (directory.size() >= 2 && directory.front() == '"' && directory.back() == '"')
      directory = directory.substr(1, directory.size() - 2);
    if (directory.empty())
      continue;
    std::error_code error;
    auto candidate = std::filesystem::path(directory) / ENCODER_NAME;
    if (std::filesystem::is_regular_file(candidate, error))
      return candidate;
  }
  return std::nullopt;
}
auto VideoRecorder::IsOpen() const -> bool {
  return input != nullptr;
}
#ifdef _WIN32
auto VideoRecorder::Start(const std::filesystem::path &encoder, const std::filesystem::path &output, const int w, const int h, const int rate) -> bool {
  if (IsOpen() || w <= 0 || h <= 0)
    return false;
  // Inherited by the child, so it can be its standard input; the end kept here is not, or the child
  // would hold its own input open and never see it close.
  SECURITY_ATTRIBUTES attributes{.nLength = sizeof(SECURITY_ATTRIBUTES), .bInheritHandle = TRUE};
  HANDLE read{};
  HANDLE write{};
  // A frame's worth of buffer, so a write does not wait on every few kilobytes the encoder takes.
  const auto frameBytes = static_cast<DWORD>(static_cast<size_t>(w) * h * 4);
  if (!CreatePipe(&read, &write, &attributes, frameBytes)) {
    spdlog::error("[VideoRecorder] Could not make a pipe to the encoder ({})", GetLastError());
    return false;
  }
  SetHandleInformation(write, HANDLE_FLAG_INHERIT, 0);
  STARTUPINFOW startup{.cb = sizeof(STARTUPINFOW)};
  startup.dwFlags = STARTF_USESTDHANDLES;
  startup.hStdInput = read;
  // The encoder's errors go where this process's do: the console of a debug build, nowhere in a
  // release one, which is a windowed program. What failed is also in its exit code, logged below.
  startup.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
  startup.hStdError = GetStdHandle(STD_ERROR_HANDLE);
  auto commandLine = std::format(L"\"{}\" ", encoder.wstring());
  for (const auto c : EncoderArguments(w, h, rate))
    commandLine.push_back(static_cast<wchar_t>(c));
  commandLine += std::format(L" \"{}\"", output.wstring());
  PROCESS_INFORMATION information{};
  // Without a window of its own: a release build is a windowed program, and a console started for the
  // encoder would open over the game for as long as it records.
  const auto started = CreateProcessW(encoder.c_str(), commandLine.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &startup, &information);
  CloseHandle(read);
  if (!started) {
    spdlog::error("[VideoRecorder] Could not start {} ({})", encoder.string(), GetLastError());
    CloseHandle(write);
    return false;
  }
  CloseHandle(information.hThread);
  process = information.hProcess;
  input = write;
  width = w;
  height = h;
  return true;
}
auto VideoRecorder::Write(const std::span<const uint8_t> frame) -> bool {
  if (!IsOpen())
    return false;
  for (size_t sent = 0; sent < frame.size();) {
    DWORD written{};
    const auto chunk = static_cast<DWORD>(std::min<size_t>(frame.size() - sent, 1u << 30));
    if (!WriteFile(static_cast<HANDLE>(input), frame.data() + sent, chunk, &written, nullptr) || written == 0) {
      // The encoder has gone, and took the rest of the stream with it; `Finish` says how it ended.
      spdlog::error("[VideoRecorder] The encoder stopped taking frames ({})", GetLastError());
      CloseHandle(static_cast<HANDLE>(input));
      input = nullptr;
      return false;
    }
    sent += written;
  }
  return true;
}
auto VideoRecorder::Finish() -> bool {
  if (input)
    CloseHandle(static_cast<HANDLE>(input));
  input = nullptr;
  if (!process)
    return false;
  // The end of its input is what tells the encoder the video is over; it then writes the rest out,
  // which for a long clip is a second or two.
  WaitForSingleObject(static_cast<HANDLE>(process), INFINITE);
  DWORD code{1};
  GetExitCodeProcess(static_cast<HANDLE>(process), &code);
  CloseHandle(static_cast<HANDLE>(process));
  process = nullptr;
  if (code != 0)
    spdlog::error("[VideoRecorder] The encoder exited with {}", code);
  return code == 0;
}
#else
auto VideoRecorder::Start(const std::filesystem::path &encoder, const std::filesystem::path &output, const int w, const int h, const int rate) -> bool {
  if (IsOpen() || w <= 0 || h <= 0)
    return false;
  const auto command = std::format("\"{}\" {} \"{}\"", encoder.string(), EncoderArguments(w, h, rate), output.string());
  auto *pipe = popen(command.c_str(), "w");
  if (!pipe) {
    spdlog::error("[VideoRecorder] Could not start {}", encoder.string());
    return false;
  }
  input = pipe;
  process = pipe;
  width = w;
  height = h;
  return true;
}
auto VideoRecorder::Write(const std::span<const uint8_t> frame) -> bool {
  if (!IsOpen())
    return false;
  if (std::fwrite(frame.data(), 1, frame.size(), static_cast<FILE *>(input)) != frame.size()) {
    spdlog::error("[VideoRecorder] The encoder stopped taking frames");
    return false;
  }
  return true;
}
auto VideoRecorder::Finish() -> bool {
  if (!process)
    return false;
  const auto code = pclose(static_cast<FILE *>(process));
  input = nullptr;
  process = nullptr;
  if (code != 0)
    spdlog::error("[VideoRecorder] The encoder exited with {}", code);
  return code == 0;
}
#endif
} // namespace kuki
