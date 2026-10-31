#pragma once
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/vector_float3.hpp>
#include <kuki_engine_export.h>
#include <primitive.hpp>
#include <span>
namespace kuki {
struct KUKI_ENGINE_API BoundingBox {
  glm::vec3 min{std::numeric_limits<float>::max()};
  glm::vec3 max{std::numeric_limits<float>::lowest()};
  BoundingBox GetWorldBounds(const glm::mat4 &) const;
  /// @brief Whether the box has volume: a size above epsilon along every axis.
  ///
  /// A flat box -- a plane's, a floor's -- has none and answers false, the same as an empty one.
  explicit operator bool() const;
  /// @brief Whether the box holds no point at all, as `BoundingBox{}` does: its minimum is above its
  /// maximum on some axis.
  ///
  /// Not the opposite of `operator bool`. A plane's box is flat, so it has no volume, but it holds the
  /// plane; a pool made with `BoundingBox{}` is a game saying its contents have no bounds worth
  /// fitting anything to. Telling the two apart is the only way to honour the second without
  /// throwing out every floor and wall with it.
  auto IsEmpty() const -> bool;
  static auto Calculate(std::span<const glm::vec3>) -> BoundingBox;
  static auto Calculate(std::span<const Vertex>) -> BoundingBox;
};
} // namespace kuki
