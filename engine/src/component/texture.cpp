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
  return {{"Type", type}, {"Width", width}, {"Height", height}, {"Id", id, PropertyType::Texture}};
}
void Texture::SetProperty(Property property) {
  if (std::holds_alternative<int>(property.value)) {
    auto& value = std::get<int>(property.value);
    if (property.name == "Id")
      id = value;
    else if (property.name == "Width")
      width = value;
    else if (property.name == "Height")
      height = value;
  } else if (std::holds_alternative<TextureType>(property.value)) {
    auto& value = std::get<TextureType>(property.value);
    if (property.name == "Type")
      type = value;
  }
}
} // namespace kuki
