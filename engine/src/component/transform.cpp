#include <component/transform.hpp>
namespace kuki {
void Transform::CopyTo(Transform& other) const {
  other.position = position;
  other.rotation = rotation;
  other.scale = scale;
}
} // namespace kuki
