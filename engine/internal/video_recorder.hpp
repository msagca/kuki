#pragma once
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
namespace kuki {
/// @brief Encodes frames into a video by piping them, as raw RGBA, to an `ffmpeg` process.
///
/// A process rather than a library: ffmpeg's libraries are LGPL or GPL by how they were built, and
/// libx264 is GPL, so linking them would make every game's licence and installer a question. Piped
/// to, it is a tool on the developer's machine like the compiler -- found on the `PATH`, never
/// shipped -- and nothing of it reaches a build. Raw frames rather than a folder of pictures, so
/// nothing large is left behind and no second step turns them into a video.
///
/// What `RECORD_FLAG` records through. Frames must all be the size it was started with: a raw
/// stream has no way to say the size changed.
class VideoRecorder {
public:
  VideoRecorder() = default;
  VideoRecorder(const VideoRecorder &) = delete;
  auto operator=(const VideoRecorder &) -> VideoRecorder & = delete;
  /// @brief Finishes the video if it is still open, so a run closed early still leaves one that plays.
  ~VideoRecorder();
  /// @brief Where `ffmpeg` is on the `PATH`, or nothing when it is not. Only the `PATH`: not the
  /// working directory, nor beside the executable, where a build never puts it.
  static auto FindEncoder() -> std::optional<std::filesystem::path>;
  /// @brief Starts the encoder writing `output` from frames of this size at this many a second.
  auto Start(const std::filesystem::path &encoder, const std::filesystem::path &output, const int width, const int height, const int rate) -> bool;
  /// @brief Hands the encoder one frame of `width * height * 4` bytes, top row first. False once the
  /// encoder has gone -- an output it could not write, say -- after which nothing more is sent.
  auto Write(std::span<const uint8_t>) -> bool;
  /// @brief Closes the stream and waits for the encoder to write out the rest. True when it exits
  /// cleanly, which is when the video is whole.
  auto Finish() -> bool;
  auto IsOpen() const -> bool;
  auto Width() const -> int {
    return width;
  }
  auto Height() const -> int {
    return height;
  }
private:
  /// @brief The process and the write end of its input, as the platform's own handles: a `HANDLE`
  /// each on Windows, a `FILE *` in `input` elsewhere. Opaque here so the header names no platform.
  void *process{};
  void *input{};
  int width{};
  int height{};
};
} // namespace kuki
