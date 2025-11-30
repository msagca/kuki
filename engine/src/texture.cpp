#include <component.hpp>
#include <texture.hpp>
#include <utility>
namespace kuki {
Texture::Texture()
  : IComponent(std::in_place_type<Texture>) {}
Texture::Texture(const Texture& other)
  : IComponent(other), type(other.type), width(other.width), height(other.height), id(other.id) {}
Texture& Texture::operator=(const Texture& other) {
  type = other.type;
  width = other.width;
  height = other.height;
  id = other.id;
  return *this;
}
} // namespace kuki
