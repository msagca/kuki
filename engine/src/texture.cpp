#include <component.hpp>
#include <texture.hpp>
#include <utility>
namespace kuki {
Texture::Texture()
  : IComponent(std::in_place_type<Texture>) {}
bool Texture::IsValid() {
  return id != 0;
}
} // namespace kuki
