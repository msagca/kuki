#include <archetype.hpp>
namespace kuki {
bool MeshMaterial::operator==(const MeshMaterial &other) const {
  return material == other.material && mesh == other.mesh;
}
} // namespace kuki
