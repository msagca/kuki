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
  if (auto value = std::get_if<int>(&property.value)) {
    if (property.name == "Id")
      id = *value;
    else if (property.name == "Width")
      width = *value;
    else if (property.name == "Height")
      height = *value;
  } else if (auto value = std::get_if<TextureType>(&property.value)) {
    if (property.name == "Type")
      type = *value;
  }
}
} // namespace kuki
