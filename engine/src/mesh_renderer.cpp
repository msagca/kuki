#include <component.hpp>
#include <mesh_renderer.hpp>
#include <utility>
namespace kuki {
MeshRenderer::MeshRenderer()
  : IComponent(std::in_place_type<MeshRenderer>) {}
} // namespace kuki
