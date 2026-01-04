#pragma once
#include <bounding_box.hpp>
#include <kuki_engine_export.h>
#include <plane.hpp>
namespace kuki {
struct KUKI_ENGINE_API Frustum {
  Plane top{};
  Plane bottom{};
  Plane right{};
  Plane left{};
  Plane far{};
  Plane near{};
  auto InFrustum(const BoundingBox &) const -> bool;
};
} // namespace kuki
