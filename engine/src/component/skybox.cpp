#include <component/component.hpp>
#include <component/skybox.hpp>
#include <string>
#include <variant>
#include <vector>
#include <glm/ext/vector_uint3.hpp>
namespace kuki {
const std::string Skybox::GetName() const {
  return ComponentTraits<Skybox>::GetName();
}
std::vector<Property> Skybox::GetProperties() const {
  return {{"Texture", id, PropertyType::Skybox}};
}
void Skybox::SetProperty(Property property) {
  if (auto value = std::get_if<glm::uvec3>(&property.value)) {
    if (property.name == "Texture")
      id = *value;
  }
}
} // namespace kuki
