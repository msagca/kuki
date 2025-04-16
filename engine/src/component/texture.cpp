#include <component/component.hpp>
#include <component/texture.hpp>
#include <string>
#include <variant>
#include <vector>
namespace kuki {
const std::string Texture::GetName() const {
  return ComponentTraits<Texture>::GetName();
}
std::vector<Property> Texture::GetProperties() const {
  return {{"Type", type}, {"Id", id}};
}
void Texture::SetProperty(Property property) {
  if (std::holds_alternative<int>(property.value)) {
    auto& value = std::get<int>(property.value);
    id = value;
  } else if (std::holds_alternative<TextureType>(property.value)) {
    auto& value = std::get<TextureType>(property.value);
    type = value;
  }
}
} // namespace kuki
