#include <bounding_box.hpp>
#include <frustum.hpp>
namespace kuki {
bool Frustum::InFrustum(const BoundingBox& bounds) const {
  const Plane planes[6] = {near, far, right, left, top, bottom};
  for (const auto& plane : planes)
    if (!plane.OnPositiveSide(bounds))
      return false;
  return true;
}
} // namespace kuki
