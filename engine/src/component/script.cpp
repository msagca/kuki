#include <component/component.hpp>
#include <component/script.hpp>
#include <string>
#include <variant>
#include <vector>
#include <application.hpp>
namespace kuki {
const std::string Script::GetName() const {
  return ComponentTraits<Script>::GetName();
}
std::vector<Property> Script::GetProperties() const {
  return {{"Id", id, PropertyType::Script}};
}
void Script::SetProperty(Property property) {
  if (std::holds_alternative<int>(property.value)) {
    auto& value = std::get<int>(property.value);
    if (property.name == "Id")
      id = value;
  }
}
IScript::IScript(Application& app, unsigned int entityId)
  : app(app), entityId(entityId) {}
} // namespace kuki
