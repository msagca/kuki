#pragma once
#include <bounding_box.hpp>
#include <glm/ext/vector_float3.hpp>
#include <kuki_engine_export.h>
#include <transform.hpp>
namespace kuki {
struct KUKI_ENGINE_API Plane {
  glm::vec3 point{};
  glm::vec3 normal{.0f, 1.0f, .0f};
  Plane();
  Plane(const glm::vec3 &, const glm::vec3 &);
  Plane(const glm::vec4 &);
  bool OnPositiveSide(const BoundingBox &) const;
  float SignedDistance(const glm::vec3 &) const;
};
} // namespace kuki
