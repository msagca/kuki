#include <component/component.hpp>
#include <component/texture.hpp>
#include <string>
#include <variant>
#include <vector>
std::string Texture::GetName() const {
  return "Texture";
}
std::vector<Property> Texture::GetProperties() const {
  return {{"Type", type}, {"ID", id}};
}
void Texture::SetProperty(Property property) {
  if (std::holds_alternative<unsigned int>(property.value)) {
    auto& value = std::get<unsigned int>(property.value);
    if (property.name == "ID")
      id = value;
  } else if (std::holds_alternative<TextureType>(property.value)) {
    auto& value = std::get<TextureType>(property.value);
    if (property.name == "Type")
      type = value;
  }
}
