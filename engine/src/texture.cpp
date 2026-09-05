#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <format>
#include <fstream>
#include <functional>
#include <launch_path.hpp>
#include <ostream>
#include <spdlog/spdlog.h>
#include <stb_dxt.h>
#include <stb_image.h>
#include <stb_image_resize2.h>
#include <string_view>
#include <system_error>
#include <texture.hpp>
#include <thread>
#include <tinyexr.h>
#include <variant>
#include <vector>
namespace kuki {
namespace {
  /// @brief Largest blob a baked texture may declare, as a guard against a corrupt header.
  ///
  /// A 16K RGBA source compresses well under this; anything past it is a damaged file asking for
  /// an allocation that would fail far less gracefully than a cache miss.
  constexpr uint64_t MAX_CACHED_BLOB_BYTES = 1ull << 32;
} // namespace
namespace {
  /// @brief Smallest side a level may have and still be worth a block format.
  ///
  /// A block covers four pixels a side whatever the level's size, so a 2x2 level costs the same
  /// eight bytes a 4x4 one does and a 1x1 level costs those eight bytes to say one colour. The chain
  /// stops here instead, which loses the last two levels of a distant fade in exchange for not
  /// paying more per level than the pixels are worth.
  constexpr int MIN_BLOCK_SIDE = 4;
  auto PixelCount(const Texture &texture) -> size_t {
    return static_cast<size_t>(texture.width) * static_cast<size_t>(texture.height) * static_cast<size_t>(texture.channels);
  }
  auto BlockBytes(const TextureCompression compression) -> size_t {
    switch (compression) {
    case TextureCompression::BC1:
    case TextureCompression::BC4:
      return 8;
    case TextureCompression::BC3:
    case TextureCompression::BC5:
      return 16;
    default:
      return 0;
    }
  }
  /// @brief Whether every pixel carries the same value in red, green and blue.
  ///
  /// Decides whether a mask may go to BC4, which keeps one channel and maps it onto the rest at the
  /// view. Occlusion, roughness and metalness are each read from a different channel, so a texture
  /// bound to more than one of those slots at once is carrying three masks and would lose two of
  /// them. Rather than guess from the slot, this asks the pixels.
  auto IsGreyscale(const std::vector<unsigned char> &pixels, const int channels) -> bool {
    if (channels < 3)
      return true;
    for (size_t i = 0; i + static_cast<size_t>(channels) <= pixels.size(); i += static_cast<size_t>(channels))
      if (pixels[i] != pixels[i + 1] || pixels[i] != pixels[i + 2])
        return false;
    return true;
  }
  auto HasTransparency(const std::vector<unsigned char> &pixels, const int channels) -> bool {
    if (channels < 4)
      return false;
    for (size_t i = 3; i < pixels.size(); i += static_cast<size_t>(channels))
      if (pixels[i] != 255)
        return true;
    return false;
  }
  auto ChooseCompression(const Texture &texture, const std::vector<unsigned char> &pixels) -> TextureCompression {
    switch (texture.content) {
    case TextureContent::Normal:
      return TextureCompression::BC5;
    case TextureContent::Occlusion:
    case TextureContent::Roughness:
    case TextureContent::Metalness:
      return IsGreyscale(pixels, texture.channels) ? TextureCompression::BC4 : TextureCompression::BC1;
    default:
      return HasTransparency(pixels, texture.channels) ? TextureCompression::BC3 : TextureCompression::BC1;
    }
  }
  /// @brief Halves a level, in the space the texture's values actually live in.
  ///
  /// Averaging sRGB bytes as though they were linear darkens every mip, which shows up as a surface
  /// that dims as it recedes. Masks and normals are already linear and must not be gamma-corrected
  /// on the way through.
  auto HalveLevel(const Texture &texture, const std::vector<unsigned char> &source, const int width, const int height, const int halfWidth, const int halfHeight) -> std::vector<unsigned char> {
    std::vector<unsigned char> half(static_cast<size_t>(halfWidth) * halfHeight * texture.channels);
    const auto layout = static_cast<stbir_pixel_layout>(texture.channels);
    if (texture.color == ColorSpace::sRGB)
      stbir_resize_uint8_srgb(source.data(), width, height, 0, half.data(), halfWidth, halfHeight, 0, layout);
    else
      stbir_resize_uint8_linear(source.data(), width, height, 0, half.data(), halfWidth, halfHeight, 0, layout);
    return half;
  }
  /// @brief Encodes a band of block rows, padding the edge blocks by repeating the last row and column.
  auto CompressBlockRows(const TextureCompression compression, const std::vector<unsigned char> &source, const int width, const int height, const int channels, unsigned char *destination, const int firstRow, const int lastRow) -> void {
    const auto blocksWide = (width + 3) / 4;
    const auto blockBytes = BlockBytes(compression);
    for (auto blockY = firstRow; blockY < lastRow; ++blockY)
      for (auto blockX = 0; blockX < blocksWide; ++blockX) {
        unsigned char block[64]{};
        for (auto y = 0; y < 4; ++y)
          for (auto x = 0; x < 4; ++x) {
            const auto sampleX = std::min(blockX * 4 + x, width - 1);
            const auto sampleY = std::min(blockY * 4 + y, height - 1);
            const auto *pixel = source.data() + (static_cast<size_t>(sampleY) * width + sampleX) * channels;
            auto *texel = block + (static_cast<size_t>(y) * 4 + x) * 4;
            texel[0] = pixel[0];
            texel[1] = channels > 1 ? pixel[1] : pixel[0];
            texel[2] = channels > 2 ? pixel[2] : pixel[0];
            texel[3] = channels > 3 ? pixel[3] : 255;
          }
        auto *target = destination + (static_cast<size_t>(blockY) * blocksWide + blockX) * blockBytes;
        switch (compression) {
        case TextureCompression::BC1:
          stb_compress_dxt_block(target, block, 0, STB_DXT_HIGHQUAL);
          break;
        case TextureCompression::BC3:
          stb_compress_dxt_block(target, block, 1, STB_DXT_HIGHQUAL);
          break;
        case TextureCompression::BC4: {
          unsigned char red[16];
          for (auto i = 0; i < 16; ++i)
            red[i] = block[i * 4];
          stb_compress_bc4_block(target, red);
          break;
        }
        case TextureCompression::BC5: {
          unsigned char redGreen[32];
          for (auto i = 0; i < 16; ++i) {
            redGreen[i * 2] = block[i * 4];
            redGreen[i * 2 + 1] = block[i * 4 + 1];
          }
          stb_compress_bc5_block(target, redGreen);
          break;
        }
        default:
          break;
        }
      }
  }
  /// @brief Encodes one level, splitting its block rows across the cores that are going spare.
  ///
  /// Encoding a 4K texture costs hundreds of milliseconds optimised and seconds unoptimised, which
  /// is several times what decoding it costs, and a model brings a hundred of them. Blocks never
  /// read each other, so bands of rows divide with no sharing beyond the read-only source.
  auto CompressLevel(const TextureCompression compression, const std::vector<unsigned char> &source, const int width, const int height, const int channels, unsigned char *destination) -> void {
    const auto blocksHigh = (height + 3) / 4;
    const auto cores = static_cast<int>(std::max(1u, std::thread::hardware_concurrency()));
    const auto workers = std::min(cores, blocksHigh);
    if (workers <= 1) {
      CompressBlockRows(compression, source, width, height, channels, destination, 0, blocksHigh);
      return;
    }
    std::vector<std::thread> threads;
    threads.reserve(static_cast<size_t>(workers) - 1);
    const auto rowsEach = (blocksHigh + workers - 1) / workers;
    for (auto worker = 1; worker < workers; ++worker) {
      const auto firstRow = worker * rowsEach;
      const auto lastRow = std::min(firstRow + rowsEach, blocksHigh);
      if (firstRow >= lastRow)
        break;
      threads.emplace_back([&, firstRow, lastRow] { CompressBlockRows(compression, source, width, height, channels, destination, firstRow, lastRow); });
    }
    CompressBlockRows(compression, source, width, height, channels, destination, 0, std::min(rowsEach, blocksHigh));
    for (auto &thread : threads)
      thread.join();
  }
  auto ReadEXR(Texture &texture) -> bool {
    float *data = nullptr;
    const char *errMsg = nullptr;
    if (LoadEXR(&data, &texture.width, &texture.height, texture.source.c_str(), &errMsg) != TINYEXR_SUCCESS) {
      if (errMsg) {
        spdlog::error("[Texture] {}", errMsg);
        FreeEXRErrorMessage(errMsg);
      } else
        spdlog::error("[Texture] failed to read {}", texture.source);
      return false;
    }
    if (!data)
      return false;
    texture.channels = 4;
    texture.range = ColorRange::HDR;
    auto &pixels = texture.data.emplace<std::vector<float>>();
    pixels.assign(data, data + PixelCount(texture));
    free(data);
    return true;
  }
  auto ReadHDR(Texture &texture) -> bool {
    auto *data = stbi_loadf(texture.source.c_str(), &texture.width, &texture.height, &texture.channels, 0);
    if (!data) {
      spdlog::error("[Texture] failed to read {}", texture.source);
      return false;
    }
    texture.range = ColorRange::HDR;
    auto &pixels = texture.data.emplace<std::vector<float>>();
    pixels.assign(data, data + PixelCount(texture));
    stbi_image_free(data);
    return true;
  }
  auto ReadLDR(Texture &texture) -> bool {
    auto *data = stbi_load(texture.source.c_str(), &texture.width, &texture.height, &texture.channels, 0);
    if (!data) {
      spdlog::error("[Texture] failed to read {}", texture.source);
      return false;
    }
    texture.range = ColorRange::LDR;
    auto &pixels = texture.data.emplace<std::vector<unsigned char>>();
    pixels.assign(data, data + PixelCount(texture));
    stbi_image_free(data);
    return true;
  }
} // namespace
namespace {
  constexpr char CACHE_MAGIC[4]{'K', 'T', 'E', 'X'};
  constexpr uint64_t FNV_OFFSET = 14695981039346656037ull;
  constexpr uint64_t FNV_PRIME = 1099511628211ull;
  /// @brief FNV-1a over the bake's inputs.
  ///
  /// Hand-rolled rather than `std::hash` because the result names a file that outlives the process:
  /// `std::hash` is only required to be consistent within one run, so a standard library free to
  /// salt it would orphan every bake on the next launch.
  auto HashBytes(uint64_t hash, const void *data, const size_t size) -> uint64_t {
    const auto *bytes = static_cast<const unsigned char *>(data);
    for (size_t i = 0; i < size; ++i) {
      hash ^= bytes[i];
      hash *= FNV_PRIME;
    }
    return hash;
  }
  template <typename T>
  auto WriteScalar(std::ostream &out, const T value) -> void {
    out.write(reinterpret_cast<const char *>(&value), sizeof(T));
  }
  template <typename T>
  auto ReadScalar(std::istream &in, T &value) -> bool {
    in.read(reinterpret_cast<char *>(&value), sizeof(T));
    return static_cast<bool>(in);
  }
  /// @brief Size and modification time of the source, which is what makes a bake stale.
  ///
  /// A hash of the pixels would be exact, but reading the file to decide whether to avoid reading
  /// the file defeats the point. Size plus timestamp is what every build system relies on.
  auto GetSourceStamp(const std::string &source, uint64_t &size, int64_t &time) -> bool {
    std::error_code ec;
    const auto path = std::filesystem::path(source);
    const auto bytes = std::filesystem::file_size(path, ec);
    if (ec)
      return false;
    const auto written = std::filesystem::last_write_time(path, ec);
    if (ec)
      return false;
    size = static_cast<uint64_t>(bytes);
    time = written.time_since_epoch().count();
    return true;
  }
} // namespace
auto GetTextureCachePath(const Texture &texture) -> std::filesystem::path {
  if (texture.source.empty())
    return {};
  auto hash = HashBytes(FNV_OFFSET, texture.source.data(), texture.source.size());
  const auto content = static_cast<uint8_t>(texture.content);
  const auto color = static_cast<uint8_t>(texture.color);
  const auto range = static_cast<uint8_t>(texture.range);
  const auto flipY = static_cast<uint8_t>(texture.flipY);
  hash = HashBytes(hash, &content, sizeof(content));
  hash = HashBytes(hash, &color, sizeof(color));
  hash = HashBytes(hash, &range, sizeof(range));
  hash = HashBytes(hash, &flipY, sizeof(flipY));
  hash = HashBytes(hash, &TEXTURE_CACHE_VERSION, sizeof(TEXTURE_CACHE_VERSION));
  return GetLaunchPath() / "cache" / "texture" / (std::format("{:016x}", hash) + ".ktex");
}
auto ReadCachedTexture(Texture &texture) -> bool {
  const auto path = GetTextureCachePath(texture);
  if (path.empty())
    return false;
  uint64_t sourceBytes{};
  int64_t sourceTime{};
  if (!GetSourceStamp(texture.source, sourceBytes, sourceTime))
    return false;
  std::ifstream in(path, std::ios::binary);
  if (!in)
    return false;
  char magic[4]{};
  in.read(magic, sizeof(magic));
  if (!in || std::memcmp(magic, CACHE_MAGIC, sizeof(magic)) != 0)
    return false;
  uint32_t version{};
  uint64_t bakedBytes{};
  int64_t bakedTime{};
  if (!ReadScalar(in, version) || version != TEXTURE_CACHE_VERSION)
    return false;
  if (!ReadScalar(in, bakedBytes) || !ReadScalar(in, bakedTime))
    return false;
  // the source has been edited or replaced since this was baked, so the bake describes an old image
  if (bakedBytes != sourceBytes || bakedTime != sourceTime)
    return false;
  int32_t width{}, height{}, channels{};
  uint8_t range{}, color{}, content{}, compression{}, flipY{};
  uint32_t levelCount{};
  uint64_t blobBytes{};
  if (!ReadScalar(in, width) || !ReadScalar(in, height) || !ReadScalar(in, channels))
    return false;
  if (!ReadScalar(in, range) || !ReadScalar(in, color) || !ReadScalar(in, content) || !ReadScalar(in, compression) || !ReadScalar(in, flipY))
    return false;
  if (!ReadScalar(in, levelCount) || !ReadScalar(in, blobBytes))
    return false;
  if (levelCount == 0 || levelCount > 32 || blobBytes == 0 || blobBytes > MAX_CACHED_BLOB_BYTES)
    return false;
  std::vector<TextureLevel> levels(levelCount);
  auto expected = size_t{};
  for (auto &level : levels) {
    int32_t levelWidth{}, levelHeight{};
    uint64_t offset{}, bytes{};
    if (!ReadScalar(in, levelWidth) || !ReadScalar(in, levelHeight) || !ReadScalar(in, offset) || !ReadScalar(in, bytes))
      return false;
    // the table has to describe one contiguous run, which is what both backends upload from
    if (offset != expected || bytes == 0 || offset + bytes > blobBytes)
      return false;
    level = {.width = levelWidth, .height = levelHeight, .offset = static_cast<size_t>(offset), .bytes = static_cast<size_t>(bytes)};
    expected += static_cast<size_t>(bytes);
  }
  if (expected != blobBytes)
    return false;
  std::vector<unsigned char> blob(static_cast<size_t>(blobBytes));
  in.read(reinterpret_cast<char *>(blob.data()), static_cast<std::streamsize>(blobBytes));
  if (in.gcount() != static_cast<std::streamsize>(blobBytes))
    return false;
  texture.width = width;
  texture.height = height;
  texture.channels = channels;
  texture.range = static_cast<ColorRange>(range);
  texture.color = static_cast<ColorSpace>(color);
  texture.content = static_cast<TextureContent>(content);
  texture.compression = static_cast<TextureCompression>(compression);
  texture.flipY = flipY != 0;
  texture.levels = std::move(levels);
  texture.data.emplace<std::vector<unsigned char>>(std::move(blob));
  return true;
}
auto WriteCachedTexture(const Texture &texture) -> bool {
  if (texture.compression == TextureCompression::None || texture.levels.empty())
    return false;
  const auto *blob = std::get_if<std::vector<unsigned char>>(&texture.data);
  if (!blob || blob->empty())
    return false;
  const auto path = GetTextureCachePath(texture);
  if (path.empty())
    return false;
  uint64_t sourceBytes{};
  int64_t sourceTime{};
  if (!GetSourceStamp(texture.source, sourceBytes, sourceTime))
    return false;
  std::error_code ec;
  std::filesystem::create_directories(path.parent_path(), ec);
  if (ec)
    return false;
  // a distinct temporary per writer, so two threads baking the same texture cannot share one
  auto temporary = path;
  temporary += std::format(".{:x}.tmp", std::hash<std::thread::id>{}(std::this_thread::get_id()));
  {
    std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
    if (!out)
      return false;
    out.write(CACHE_MAGIC, sizeof(CACHE_MAGIC));
    WriteScalar(out, TEXTURE_CACHE_VERSION);
    WriteScalar(out, sourceBytes);
    WriteScalar(out, sourceTime);
    WriteScalar(out, static_cast<int32_t>(texture.width));
    WriteScalar(out, static_cast<int32_t>(texture.height));
    WriteScalar(out, static_cast<int32_t>(texture.channels));
    WriteScalar(out, static_cast<uint8_t>(texture.range));
    WriteScalar(out, static_cast<uint8_t>(texture.color));
    WriteScalar(out, static_cast<uint8_t>(texture.content));
    WriteScalar(out, static_cast<uint8_t>(texture.compression));
    WriteScalar(out, static_cast<uint8_t>(texture.flipY ? 1 : 0));
    WriteScalar(out, static_cast<uint32_t>(texture.levels.size()));
    WriteScalar(out, static_cast<uint64_t>(blob->size()));
    for (const auto &level : texture.levels) {
      WriteScalar(out, static_cast<int32_t>(level.width));
      WriteScalar(out, static_cast<int32_t>(level.height));
      WriteScalar(out, static_cast<uint64_t>(level.offset));
      WriteScalar(out, static_cast<uint64_t>(level.bytes));
    }
    out.write(reinterpret_cast<const char *>(blob->data()), static_cast<std::streamsize>(blob->size()));
    if (!out)
      return false;
  }
  std::filesystem::rename(temporary, path, ec);
  if (ec) {
    // losing the race is fine: whoever won wrote the same bytes this would have
    std::filesystem::remove(temporary, ec);
    return false;
  }
  // logged on the write and not on the read, so a cold run says what it is spending its time on
  // and a warm run stays silent, which is the difference the reader is actually looking for
  spdlog::info("[Texture] baked {} ({} KB, {} levels)", std::filesystem::path(texture.source).filename().string(), blob->size() / 1024, texture.levels.size());
  return true;
}
auto PrepareTextureCached(Texture &texture) -> bool {
  if (ReadCachedTexture(texture))
    return true;
  if (!ReadTexturePixels(texture))
    return false;
  if (PrepareTexturePixels(texture))
    WriteCachedTexture(texture);
  return true;
}
auto ResolveTexturePath(const std::filesystem::path &modelDir, const std::string &recorded) -> std::filesystem::path {
  static constexpr std::array<std::string_view, 5> TEXTURE_FOLDERS{"textures", "Textures", "texture", "maps", "Maps"};
  if (recorded.empty())
    return {};
  std::error_code ec;
  const auto Found = [&ec](const std::filesystem::path &candidate) { return std::filesystem::is_regular_file(candidate, ec); };
  auto portable = recorded;
  std::replace(portable.begin(), portable.end(), '\\', '/');
  const auto recordedPath = std::filesystem::path(portable);
  if (Found(recordedPath))
    return recordedPath.lexically_normal();
  if (Found(modelDir / recordedPath))
    return (modelDir / recordedPath).lexically_normal();
  const auto fileName = recordedPath.filename();
  if (fileName.empty())
    return {};
  std::vector<std::filesystem::path> roots{modelDir};
  if (auto parent = modelDir.parent_path(); !parent.empty() && parent != modelDir)
    roots.push_back(parent);
  const auto recordedFolder = recordedPath.parent_path().filename();
  for (const auto &root : roots) {
    if (!recordedFolder.empty() && Found(root / recordedFolder / fileName))
      return (root / recordedFolder / fileName).lexically_normal();
    if (Found(root / fileName))
      return (root / fileName).lexically_normal();
    for (const auto folder : TEXTURE_FOLDERS)
      if (Found(root / folder / fileName))
        return (root / folder / fileName).lexically_normal();
  }
  return {};
}
auto HasTexturePixels(const Texture &texture) -> bool {
  return std::visit([](const auto &pixels) { return !pixels.empty(); }, texture.data);
}
auto ReadTexturePixels(Texture &texture) -> bool {
  if (texture.source.empty())
    return false;
  texture.compression = TextureCompression::None;
  texture.levels.clear();
  const auto extension = std::filesystem::path(texture.source).extension().string();
  if (extension == ".exr")
    return ReadEXR(texture);
  if (extension == ".hdr")
    return ReadHDR(texture);
  return ReadLDR(texture);
}
auto EnsureTexturePixels(Texture &texture) -> bool {
  return HasTexturePixels(texture) || ReadTexturePixels(texture);
}
auto ReleaseTexturePixels(Texture &texture) -> bool {
  if (texture.source.empty() || !HasTexturePixels(texture))
    return false;
  texture.data.emplace<std::vector<unsigned char>>();
  texture.compression = TextureCompression::None;
  texture.levels.clear();
  return true;
}
auto GetCompressedLevelBytes(const TextureCompression compression, const int width, const int height) -> size_t {
  const auto blocksWide = static_cast<size_t>((width + 3) / 4);
  const auto blocksHigh = static_cast<size_t>((height + 3) / 4);
  return blocksWide * blocksHigh * BlockBytes(compression);
}
auto GetTextureLevels(const Texture &texture) -> std::vector<TextureLevel> {
  if (!texture.levels.empty())
    return texture.levels;
  return {TextureLevel{.width = texture.width, .height = texture.height, .offset = 0, .bytes = std::visit([](const auto &pixels) { return pixels.size() * sizeof(pixels[0]); }, texture.data)}};
}
auto PrepareTexturePixels(Texture &texture) -> bool {
  if (!texture.levels.empty())
    return texture.compression != TextureCompression::None;
  if (texture.range == ColorRange::HDR || texture.content == TextureContent::Skybox)
    return false;
  auto *pixels = std::get_if<std::vector<unsigned char>>(&texture.data);
  if (!pixels || pixels->empty() || texture.channels < 1 || texture.channels > 4)
    return false;
  if (texture.width % 4 != 0 || texture.height % 4 != 0)
    return false;
  const auto compression = ChooseCompression(texture, *pixels);
  std::vector<std::vector<unsigned char>> raw;
  std::vector<TextureLevel> levels;
  raw.push_back(std::move(*pixels));
  auto width = texture.width;
  auto height = texture.height;
  auto total = size_t{};
  while (true) {
    const auto bytes = GetCompressedLevelBytes(compression, width, height);
    levels.push_back({.width = width, .height = height, .offset = total, .bytes = bytes});
    total += bytes;
    const auto halfWidth = width / 2;
    const auto halfHeight = height / 2;
    if (halfWidth < MIN_BLOCK_SIDE || halfHeight < MIN_BLOCK_SIDE)
      break;
    raw.push_back(HalveLevel(texture, raw.back(), width, height, halfWidth, halfHeight));
    width = halfWidth;
    height = halfHeight;
  }
  std::vector<unsigned char> blocks(total);
  for (size_t level = 0; level < raw.size(); ++level)
    CompressLevel(compression, raw[level], levels[level].width, levels[level].height, texture.channels, blocks.data() + levels[level].offset);
  texture.data.emplace<std::vector<unsigned char>>(std::move(blocks));
  texture.levels = std::move(levels);
  texture.compression = compression;
  return true;
}
} // namespace kuki
