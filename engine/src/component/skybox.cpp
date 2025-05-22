#include <component/component.hpp>
#include <component/skybox.hpp>
#include <string>
#include <variant>
#include <vector>
namespace kuki {
const std::string Skybox::GetName() const {
  return ComponentTraits<Skybox>::GetName();
}
std::vector<Property> Skybox::GetProperties() const {
  return {{"Data", data, PropertyType::Skybox}};
}
void Skybox::SetProperty(Property property) {
  if (auto value = std::get_if<SkyboxData>(&property.value)) {
    if (property.name == "Data")
      data = *value;
  }
}
} // namespace kuki
