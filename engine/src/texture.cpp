#include <component.hpp>
#include <texture.hpp>
#include <utility>
namespace kuki {
Texture::Texture()
  : IComponent(std::in_place_type<Texture>) {}
} // namespace kuki
