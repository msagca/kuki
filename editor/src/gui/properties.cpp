#include <application.hpp>
#include <component/component.hpp>
#include <editor.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>
#include <string>
#include <variant>
void Editor::DisplayProperties() {
  if (selectedEntity < 0)
    return;
  ImGui::Begin("Properties");
  static IComponent* selectedComponent = nullptr;
  auto components = GetAllComponents(selectedEntity);
  for (auto i = 0; i < components.size(); ++i) {
    const auto& component = components[i];
    auto isSelected = (selectedComponent == component);
    ImGui::PushID(static_cast<int>(i));
    auto name = component->GetName();
    if (ImGui::Selectable(name.c_str(), isSelected)) {
      if (isSelected)
        selectedComponent = nullptr;
      else
        selectedComponent = GetComponent(selectedEntity, name);
    }
    if (ImGui::BeginPopupContextItem()) {
      if (ImGui::MenuItem("Remove")) {
        RemoveComponent(selectedEntity, name);
        if (selectedComponent == component)
          selectedComponent = nullptr;
      }
      ImGui::EndPopup();
    }
    auto properties = component->GetProperties();
    for (auto j = 0; j < properties.size(); ++j) {
      auto& prop = properties[j];
      auto value = prop.value;
      ImGui::PushID(static_cast<int>(j));
      if (std::holds_alternative<glm::vec3>(value)) {
        auto valueVec3 = std::get<glm::vec3>(value);
        if (prop.type == PropertyType::Color) {
          if (ImGui::ColorEdit3(prop.name.c_str(), glm::value_ptr(valueVec3)))
            component->SetProperty(Property(prop.name, valueVec3));
        } else if (ImGui::InputFloat3(prop.name.c_str(), glm::value_ptr(valueVec3)))
          component->SetProperty(Property(prop.name, valueVec3));
      } else if (std::holds_alternative<glm::vec4>(value)) {
        auto valueVec4 = std::get<glm::vec4>(value);
        if (prop.type == PropertyType::Color) {
          if (ImGui::ColorEdit4(prop.name.c_str(), glm::value_ptr(valueVec4)))
            component->SetProperty(Property(prop.name, valueVec4));
        } else if (ImGui::InputFloat4(prop.name.c_str(), glm::value_ptr(valueVec4)))
          component->SetProperty(Property(prop.name, valueVec4));
      } else if (std::holds_alternative<int>(value)) {
        auto valueInt = std::get<int>(value);
        if (ImGui::InputInt(prop.name.c_str(), &valueInt))
          component->SetProperty(Property(prop.name, valueInt));
      } else if (std::holds_alternative<float>(value)) {
        auto valueFloat = std::get<float>(value);
        if (ImGui::InputFloat(prop.name.c_str(), &valueFloat))
          component->SetProperty(Property(prop.name, valueFloat));
      } else if (std::holds_alternative<bool>(value)) {
        auto valueBool = std::get<bool>(value);
        if (ImGui::Checkbox(prop.name.c_str(), &valueBool))
          component->SetProperty(Property(prop.name, valueBool));
      } else if (std::holds_alternative<CameraType>(value)) {
        static auto items = EnumTraits<CameraType>::GetNames();
        auto valueEnum = std::get<CameraType>(value);
        auto currentItem = static_cast<int>(valueEnum);
        if (ImGui::Combo(prop.name.c_str(), &currentItem, items.data(), items.size())) {
          valueEnum = static_cast<CameraType>(currentItem);
          component->SetProperty(Property(prop.name, valueEnum));
        }
      } else if (std::holds_alternative<LightType>(value)) {
        static auto items = EnumTraits<LightType>::GetNames();
        auto valueEnum = std::get<LightType>(value);
        auto currentItem = static_cast<int>(valueEnum);
        if (ImGui::Combo(prop.name.c_str(), &currentItem, items.data(), items.size())) {
          valueEnum = static_cast<LightType>(currentItem);
          component->SetProperty(Property(prop.name, valueEnum));
        }
      } else if (std::holds_alternative<TextureType>(value)) {
        static auto items = EnumTraits<TextureType>::GetNames();
        auto valueEnum = std::get<TextureType>(value);
        auto currentItem = static_cast<int>(valueEnum);
        if (ImGui::Combo(prop.name.c_str(), &currentItem, items.data(), items.size())) {
          valueEnum = static_cast<TextureType>(currentItem);
          component->SetProperty(Property(prop.name, valueEnum));
        }
      }
      ImGui::PopID();
    }
    ImGui::PopID();
  }
  auto availableComponents = GetMissingComponents(selectedEntity);
  for (const auto& comp : availableComponents)
    if (ImGui::Selectable(comp.c_str()))
      AddComponent(selectedEntity, comp);
  ImGui::End();
}
