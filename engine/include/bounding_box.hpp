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
  /// @brief Whether the box holds any point: false only for an empty one, as `BoundingBox{}` is, whose
  /// minimum is above its maximum.
  ///
  /// It used to ask for volume -- a size above epsilon along every axis -- which a plane's box, a
  /// floor's or a wall's, has not. Everything that asked it meant "are there bounds at all", so a
  /// floor made of the `Plane` primitive was never culled and was left out of the sun's shadow fit,
  /// and a flat node of a model never had its bounds copied from the import. Answering for volume
  /// would also make a floor indistinguishable from a pool its game made with no bounds
  /// (`PoolSettings::bounds`), which the probe volume's fit leaves out. A flat box is culled and
  /// fitted like any other; one that is a single point is a thing whose caller must not divide by
  /// its size.
  explicit operator bool() const;
  static auto Calculate(std::span<const glm::vec3>) -> BoundingBox;
  static auto Calculate(std::span<const Vertex>) -> BoundingBox;
};
} // namespace kuki
