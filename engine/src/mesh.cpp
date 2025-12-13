#include <component.hpp>
#include <mesh.hpp>
#include <utility>
namespace kuki {
Mesh::Mesh()
  : IComponent(std::in_place_type<Mesh>) {}
} // namespace kuki
