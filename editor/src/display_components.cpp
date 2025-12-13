#include <camera.hpp>
#include <component.hpp>
#include <component_traits.hpp>
#include <display_traits.hpp>
#include <editor.hpp>
#include <glfw_constants.hpp>
#include <imgui.h>
#include <light.hpp>
#include <material.hpp>
#include <mesh.hpp>
#include <mesh_filter.hpp>
#include <mesh_renderer.hpp>
#include <skybox.hpp>
#include <string>
#include <texture.hpp>
#include <transform.hpp>
using namespace kuki;
std::string Editor::GetComponentName(IComponent* component) {
  if (!component)
    return "";
  if (auto camera = component->As<Camera>())
    return DisplayTraits<Camera>::GetName();
  if (auto light = component->As<Light>())
    return DisplayTraits<Light>::GetName();
  if (auto material = component->As<Material>())
    return DisplayTraits<Material>::GetName();
  if (auto mesh = component->As<Mesh>())
    return DisplayTraits<Mesh>::GetName();
  if (auto filter = component->As<MeshFilter>())
    return DisplayTraits<MeshFilter>::GetName();
  if (auto renderer = component->As<MeshRenderer>())
    return DisplayTraits<MeshRenderer>::GetName();
  if (auto skybox = component->As<Skybox>())
    return DisplayTraits<Skybox>::GetName();
  if (auto texture = component->As<Texture>())
    return DisplayTraits<Texture>::GetName();
  if (auto transform = component->As<Transform>())
    return DisplayTraits<Transform>::GetName();
  return "";
}
ComponentType Editor::GetComponentType(IComponent* component) {
  if (!component)
    return ComponentType::Unknown;
  if (auto camera = component->As<Camera>())
    return ComponentTraits<Camera>::GetType();
  if (auto light = component->As<Light>())
    return ComponentTraits<Light>::GetType();
  if (auto material = component->As<Material>())
    return ComponentTraits<Material>::GetType();
  if (auto mesh = component->As<Mesh>())
    return ComponentTraits<Mesh>::GetType();
  if (auto filter = component->As<MeshFilter>())
    return ComponentTraits<MeshFilter>::GetType();
  if (auto renderer = component->As<MeshRenderer>())
    return ComponentTraits<MeshRenderer>::GetType();
  if (auto skybox = component->As<Skybox>())
    return ComponentTraits<Skybox>::GetType();
  if (auto texture = component->As<Texture>())
    return ComponentTraits<Texture>::GetType();
  if (auto transform = component->As<Transform>())
    return ComponentTraits<Transform>::GetType();
  return ComponentType::Unknown;
}
void Editor::DisplayComponents() {
  if (!context.selectedEntity.IsValid())
    return;
  ImGui::Begin("Properties");
  const auto clicked = ImGui::IsMouseClicked(ImGuiMouseButton_Left);
  const auto windowHovered = ImGui::IsWindowHovered();
  const auto itemsHovered = ImGui::IsAnyItemHovered();
  if (clicked && windowHovered && !itemsHovered)
    context.assetMask = -1;
  auto components = GetAllEntityComponents(context.selectedEntity);
  for (auto i = 0; i < components.size(); ++i) {
    const auto& component = components[i];
    const auto componentType = static_cast<int>(GetComponentType(component));
    const auto isSelected = context.selectedComponent == componentType;
    ImGui::PushID(static_cast<int>(i));
    const auto name = GetComponentName(component);
    if (ImGui::Selectable(name.c_str(), isSelected))
      context.selectedComponent = componentType;
    auto removed = false;
    if (ImGui::BeginPopupContextItem()) {
      if (ImGui::MenuItem("Remove")) {
        RemoveEntityComponent(context.selectedEntity, static_cast<ComponentType>(componentType));
        if (context.selectedComponent == componentType)
          context.selectedComponent = -1;
        removed = true;
      }
      ImGui::EndPopup();
    }
    if (!removed)
      DisplayProperties(component);
    ImGui::PopID();
    if (removed) {
      ImGui::End();
      return;
    }
  }
  if (ImGui::BeginPopupContextWindow("Add", ImGuiPopupFlags_NoOpenOverItems | ImGuiPopupFlags_MouseButtonRight)) {
    auto availableComponents = GetMissingEntityComponents(context.selectedEntity);
    for (const auto& comp : availableComponents)
      if (ImGui::MenuItem(comp.c_str()))
        AddEntityComponent(context.selectedEntity, comp);
    ImGui::EndPopup();
  }
  ImGui::End();
}
