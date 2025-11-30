#include <component.hpp>
#include <mesh_filter.hpp>
#include <utility>
namespace kuki {
MeshFilter::MeshFilter()
  : IComponent(std::in_place_type<MeshFilter>) {}
} // namespace kuki
