#include <component/texture.hpp>
namespace kuki {
void Texture::CopyTo(Texture& other) const {
  other.id = id;
  other.type = type;
  other.width = width;
  other.height = height;
}
} // namespace kuki
