#include <component/component.hpp>
#include <component/light.hpp>
#include <glm/ext/vector_float3.hpp>
#include <string>
#include <variant>
#include <vector>
std::string Light::GetName() const {
  return "Light";
}
std::vector<Property> Light::GetProperties() const {
  return {{"Type", type}, {"Vector", vector}, {"Ambient", ambient}, {"Diffuse", diffuse}, {"Specular", specular}, {"Constant", constant}, {"Linear", linear}, {"Quadratic", quadratic}};
}
void Light::SetProperty(Property property) {
  if (std::holds_alternative<glm::vec3>(property.value)) {
    auto& value = std::get<glm::vec3>(property.value);
    if (property.name == "Vector")
      vector = value;
    else if (property.name == "Ambient")
      ambient = value;
    else if (property.name == "Diffuse")
      diffuse = value;
    else if (property.name == "Specular")
      specular = value;
  } else if (std::holds_alternative<float>(property.value)) {
    auto& value = std::get<float>(property.value);
    if (property.name == "Constant")
      constant = value;
    else if (property.name == "Linear")
      linear = value;
    else if (property.name == "Quadratic")
      quadratic = value;
  } else if (std::holds_alternative<LightType>(property.value))
    if (property.name == "Type")
      type = std::get<LightType>(property.value);
}
