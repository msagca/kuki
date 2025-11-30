#include <component.hpp>
#include <skybox.hpp>
#include <utility>
namespace kuki {
Skybox::Skybox()
  : IComponent(std::in_place_type<Skybox>) {}
} // namespace kuki
