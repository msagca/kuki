#include <algorithm>
#include <cmath>
#include <cstddef>
#include <glm/ext/vector_float3.hpp>
#include <glm/geometric.hpp>
#include <optional>
#include <skybox_light.hpp>
#include <spdlog/spdlog.h>
#include <texture.hpp>
#include <variant>
#include <vector>
namespace kuki {
namespace {
constexpr int BIN_COLUMNS = 64;
constexpr int BIN_ROWS = 32;
constexpr float PI = 3.14159265359f;
struct Bin {
  glm::vec3 color{};
  double weight{};
};
auto Luminance(const glm::vec3 &color) -> float {
  return glm::dot(color, glm::vec3(.2126f, .7152f, .0722f));
}
auto SampleAt(const Texture &texture, const size_t index) -> glm::vec3 {
  const auto channels = static_cast<size_t>(std::max(1, texture.channels));
  const auto base = index * channels;
  if (const auto *floats = std::get_if<std::vector<float>>(&texture.data); floats) {
    if (base + channels > floats->size())
      return {};
    const auto r = (*floats)[base];
    const auto g = channels > 1 ? (*floats)[base + 1] : r;
    const auto b = channels > 2 ? (*floats)[base + 2] : r;
    return {r, g, b};
  }
  if (const auto *bytes = std::get_if<std::vector<unsigned char>>(&texture.data); bytes) {
    if (base + channels > bytes->size())
      return {};
    const auto r = (*bytes)[base] / 255.f;
    const auto g = channels > 1 ? (*bytes)[base + 1] / 255.f : r;
    const auto b = channels > 2 ? (*bytes)[base + 2] / 255.f : r;
    return {r, g, b};
  }
  return {};
}
auto DirectionFromUV(const float u, const float v) -> glm::vec3 {
  const auto longitude = (u * 2.f - 1.f) * PI;
  const auto latitude = (v * 2.f - 1.f) * (PI * .5f);
  return {std::cos(latitude) * std::sin(longitude), std::sin(latitude), std::cos(latitude) * std::cos(longitude)};
}
} // namespace
auto ExtractDominantLight(const Texture &texture, const float minimumContrast) -> std::optional<DominantLight> {
  if (texture.width <= 0 || texture.height <= 0)
    return std::nullopt;
  const auto width = static_cast<size_t>(texture.width);
  const auto height = static_cast<size_t>(texture.height);
  std::vector<Bin> bins(static_cast<size_t>(BIN_COLUMNS) * BIN_ROWS);
  for (size_t y = 0; y < height; ++y) {
    const auto row = (static_cast<float>(y) + .5f) / height;
    const auto v = texture.flipY ? row : 1.f - row;
    const auto latitude = (v * 2.f - 1.f) * (PI * .5f);
    const auto solidAngle = std::max(std::cos(latitude), .0f);
    const auto binRow = std::min<size_t>(BIN_ROWS - 1, static_cast<size_t>(v * BIN_ROWS));
    for (size_t x = 0; x < width; ++x) {
      const auto u = (static_cast<float>(x) + .5f) / width;
      const auto binColumn = std::min<size_t>(BIN_COLUMNS - 1, static_cast<size_t>(u * BIN_COLUMNS));
      auto &bin = bins[binRow * BIN_COLUMNS + binColumn];
      bin.color += SampleAt(texture, y * width + x) * solidAngle;
      bin.weight += solidAngle;
    }
  }
  auto total = 0.0;
  auto populated = size_t{0};
  auto peakIndex = bins.size();
  auto peakLuminance = .0f;
  for (size_t i = 0; i < bins.size(); ++i) {
    if (bins[i].weight <= 0.0)
      continue;
    const auto average = bins[i].color / static_cast<float>(bins[i].weight);
    const auto luminance = Luminance(average);
    total += luminance;
    ++populated;
    if (luminance > peakLuminance) {
      peakLuminance = luminance;
      peakIndex = i;
    }
  }
  if (populated == 0 || peakIndex >= bins.size() || peakLuminance <= .0f)
    return std::nullopt;
  const auto mean = static_cast<float>(total / populated);
  if (mean <= .0f)
    return std::nullopt;
  const auto contrast = peakLuminance / mean;
  if (contrast < minimumContrast) {
    spdlog::info("[SkyboxLight] No dominant light: peak is only {:.2f}x the average, below the {:.2f}x threshold", contrast, minimumContrast);
    return std::nullopt;
  }
  const auto binColumn = peakIndex % BIN_COLUMNS;
  const auto binRow = peakIndex / BIN_COLUMNS;
  const auto u = (static_cast<float>(binColumn) + .5f) / BIN_COLUMNS;
  const auto v = (static_cast<float>(binRow) + .5f) / BIN_ROWS;
  const auto toLight = DirectionFromUV(u, v);
  DominantLight result;
  result.direction = -glm::normalize(toLight);
  const auto peakColor = bins[peakIndex].color / static_cast<float>(bins[peakIndex].weight);
  result.color = peakLuminance > .0f ? peakColor / peakLuminance : glm::vec3(1.f);
  result.contrast = contrast;
  spdlog::info("[SkyboxLight] Dominant light at {:.2f}x average, direction ({:.2f}, {:.2f}, {:.2f})", contrast, result.direction.x, result.direction.y, result.direction.z);
  return result;
}
} // namespace kuki
