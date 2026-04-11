#pragma once
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/vector_float3.hpp>
#include <kuki_engine_export.h>
#include <primitive.hpp>
#include <span>
namespace kuki {
struct KUKI_ENGINE_API BoundingBox {
  /// @brief Minimum local bounds of the mesh at scale 1
  glm::vec3 min{std::numeric_limits<float>::max()};
  /// @brief Maximum local bounds of the mesh at scale 1
  glm::vec3 max{std::numeric_limits<float>::lowest()};
  /// @brief Get the world space bounds
  BoundingBox GetWorldBounds(const glm::mat4 &) const;
  explicit operator bool() const;
  static auto Calculate(std::span<const glm::vec3>) -> BoundingBox;
  static auto Calculate(std::span<const Vertex>) -> BoundingBox;
};
} // namespace kuki
