#define GLM_ENABLE_EXPERIMENTAL
#include <application.hpp>
#include <camera_controller.hpp>
#include <component/component.hpp>
#include <component/material.hpp>
#include <component/mesh.hpp>
#include <component/transform.hpp>
#include <cstring>
#include <editor.hpp>
#include <entity_manager.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>
#include <input_manager.hpp>
#include <render_system.hpp>
#include <scene.hpp>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>
void Editor::DisplayScene() {
  static const ImVec2 uv0(.0f, 1.0f);
  static const ImVec2 uv1(1.0f, .0f);
  ImGui::Begin("Scene");
  auto contentRegion = ImGui::GetContentRegionAvail();
  auto scene = GetActiveScene();
  auto renderSystem = GetSystem<RenderSystem>();
  if (renderSystem) {
    auto texture = renderSystem->RenderSceneToTexture(cameraController.GetCamera(), contentRegion.x, contentRegion.y, selectedEntity);
    if (texture >= 0)
      ImGui::Image(texture, ImVec2(contentRegion.x, contentRegion.y), uv0, uv1);
  }
  ImGui::End();
}
void Editor::DisplayResources() {
  const static auto tileSize = ImVec2(96.0f, 96.0f);
  ImGui::Begin("Resources");
  if (ImGui::BeginPopupContextWindow()) {
    if (ImGui::BeginMenu("Import")) {
      if (ImGui::Selectable("Model"))
        fileDialog.Open();
      ImGui::EndMenu();
    }
    ImGui::EndPopup();
  }
  static std::unordered_map<unsigned int, size_t> assetToTexture;
  if (updateThumbnails)
    for (auto& pair : assetToTexture)
      texturePool.Release(pair.second);
  auto renderSystem = GetSystem<RenderSystem>();
  if (renderSystem) {
    auto contentRegion = ImGui::GetContentRegionAvail();
    auto tilesPerRow = static_cast<int>(contentRegion.x / tileSize.x);
    auto tileCount = 0;
    if (tilesPerRow > 0)
      assetManager.ForAll([&](unsigned int assetID) {
        if (assetManager.HasComponents<Transform, Mesh, Material>(assetID)) {
          ImVec2 tilePos((tileCount % tilesPerRow) * tileSize.x, (tileCount / tilesPerRow) * tileSize.y);
          ImGui::SetCursorPos(tilePos);
          auto it = assetToTexture.find(assetID);
          unsigned int textureID;
          if (it == assetToTexture.end()) {
            auto itemID = texturePool.Request();
            assetToTexture[assetID] = itemID;
            textureID = *texturePool.Get(itemID);
          } else
            textureID = *texturePool.Get(it->second);
          if (updateThumbnails)
            renderSystem->RenderAssetToTexture(assetID, textureID, tileSize.x);
          ImGui::Image(textureID, tileSize);
          tileCount++;
        }
      });
  }
  updateThumbnails = false;
  ImGui::End();
  fileDialog.Display();
  if (fileDialog.HasSelected()) {
    auto filepath = fileDialog.GetSelected();
    auto filename = filepath.stem().string();
    assetLoader.LoadModel(filename, filepath.string());
    auto scene = GetActiveScene();
    auto& spawnManager = scene->GetSpawnManager();
    spawnManager.Spawn(filename);
    fileDialog.ClearSelected();
  }
}
void Editor::DisplayHierarchy() {
  static auto clickedEntity = -1;
  static auto renameMode = false;
  static char newName[256] = "";
  ImGui::Begin("Hierarchy");
  auto scene = GetActiveScene();
  auto& entityManager = scene->GetEntityManager();
  entityManager.ForAll([&](unsigned int id) {
    if (!entityManager.HasParent(id))
      DisplayEntityHierarchy(id, selectedEntity, clickedEntity);
  });
  if (ImGui::BeginPopupContextWindow()) {
    if (clickedEntity >= 0) {
      if (ImGui::MenuItem("Remove")) {
        entityManager.Delete(clickedEntity);
        clickedEntity = -1;
        selectedEntity = -1;
      } else if (ImGui::MenuItem("Rename")) {
        strcpy_s(newName, entityManager.GetName(clickedEntity).c_str());
        renameMode = true;
      }
    } else
      DisplayCreateMenu();
    ImGui::EndPopup();
  }
  ImGui::End();
  if (renameMode) {
    inputManager.DisableKeyCallbacks();
    ImGui::Begin("Rename", &renameMode);
    ImGui::SetKeyboardFocusHere();
    if (ImGui::InputText("New Name", newName, IM_ARRAYSIZE(newName), ImGuiInputTextFlags_EnterReturnsTrue)) {
      entityManager.Rename(clickedEntity, std::string(newName));
      renameMode = false;
    }
    if (!renameMode)
      inputManager.EnableKeyCallbacks();
    ImGui::End();
  }
  if (selectedEntity >= 0)
    DisplayProperties(selectedEntity);
}
void Editor::DisplayEntityHierarchy(unsigned int id, int& selectedEntity, int& clickedEntity) {
  auto scene = GetActiveScene();
  auto& entityManager = scene->GetEntityManager();
  auto name = entityManager.GetName(id).c_str();
  auto hasChildren = entityManager.HasChildren(id);
  ImGuiTreeNodeFlags nodeFlags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth | (id == selectedEntity ? ImGuiTreeNodeFlags_Selected : 0) | (hasChildren ? ImGuiTreeNodeFlags_OpenOnArrow : ImGuiTreeNodeFlags_Leaf);
  auto nodeOpen = ImGui::TreeNodeEx((void*)(intptr_t)id, nodeFlags, "%s", name);
  if (ImGui::IsItemClicked())
    selectedEntity = id;
  if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
    clickedEntity = id;
  if (nodeOpen) {
    entityManager.ForEachChild(id, [&](unsigned int childID, std::string& _) {
      DisplayEntityHierarchy(childID, selectedEntity, clickedEntity);
    });
    ImGui::TreePop();
  }
}
void Editor::DisplayProperties(unsigned int selectedEntity) {
  ImGui::Begin("Properties");
  static IComponent* selectedComponent = nullptr;
  auto scene = GetActiveScene();
  auto& entityManager = scene->GetEntityManager();
  auto components = entityManager.GetAllComponents(selectedEntity);
  for (auto i = 0; i < components.size(); ++i) {
    const auto& component = components[i];
    auto isSelected = (selectedComponent == component);
    ImGui::PushID(static_cast<int>(i));
    if (ImGui::Selectable(component->GetName().c_str(), isSelected)) {
      if (isSelected)
        selectedComponent = nullptr;
      else
        selectedComponent = entityManager.GetComponent(selectedEntity, component->GetName());
    }
    if (ImGui::BeginPopupContextItem()) {
      if (ImGui::MenuItem("Remove")) {
        entityManager.RemoveComponent(selectedEntity, component->GetName());
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
      auto isColor = (prop.name == "Ambient" || prop.name == "Diffuse" || prop.name == "Specular");
      if (std::holds_alternative<glm::vec3>(value)) {
        auto valueVec3 = std::get<glm::vec3>(value);
        if (isColor) {
          if (ImGui::ColorEdit3(prop.name.c_str(), glm::value_ptr(valueVec3)))
            component->SetProperty(Property(prop.name, valueVec3));
        } else if (ImGui::InputFloat3(prop.name.c_str(), glm::value_ptr(valueVec3)))
          component->SetProperty(Property(prop.name, valueVec3));
      } else if (std::holds_alternative<int>(value)) {
        auto valueInt = std::get<int>(value);
        if (ImGui::InputInt(prop.name.c_str(), &valueInt))
          component->SetProperty(Property(prop.name, valueInt));
      } else if (std::holds_alternative<unsigned int>(value)) {
        auto valueInt = static_cast<int>(std::get<unsigned int>(value));
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
        auto valueEnum = std::get<CameraType>(value);
        static const char* items[] = {"Perspective", "Orthographic"};
        auto currentItem = static_cast<int>(valueEnum);
        if (ImGui::Combo(prop.name.c_str(), &currentItem, items, IM_ARRAYSIZE(items))) {
          valueEnum = static_cast<CameraType>(currentItem);
          component->SetProperty(Property(prop.name, valueEnum));
        }
      } else if (std::holds_alternative<LightType>(value)) {
        auto valueEnum = std::get<LightType>(value);
        static const char* items[] = {"Directional", "Point"};
        auto currentItem = static_cast<int>(valueEnum);
        if (ImGui::Combo(prop.name.c_str(), &currentItem, items, IM_ARRAYSIZE(items))) {
          valueEnum = static_cast<LightType>(currentItem);
          component->SetProperty(Property(prop.name, valueEnum));
        }
      } else if (std::holds_alternative<TextureType>(value)) {
        auto valueEnum = std::get<TextureType>(value);
        static const char* items[] = {"Base", "Normal", "ORM", "Metalness", "Occlusion", "Roughness"};
        auto currentItem = static_cast<int>(valueEnum);
        if (ImGui::Combo(prop.name.c_str(), &currentItem, items, IM_ARRAYSIZE(items))) {
          valueEnum = static_cast<TextureType>(currentItem);
          component->SetProperty(Property(prop.name, valueEnum));
        }
      }
      ImGui::PopID();
    }
    ImGui::PopID();
  }
  auto availableComponents = entityManager.GetMissingComponents(selectedEntity);
  for (const auto& comp : availableComponents)
    if (ImGui::Selectable(comp.c_str()))
      entityManager.AddComponent(selectedEntity, comp);
  ImGui::End();
}
void Editor::DisplayCreateMenu() {
  static const char* primitives[] = {"Cube", "Sphere", "Cylinder"};
  static auto selectedPrimitive = -1;
  ImGui::OpenPopup("Create");
  if (ImGui::BeginPopup("Create")) {
    if (ImGui::Selectable("Empty")) {
      auto scene = GetActiveScene();
      scene->GetEntityManager().Create();
      ImGui::CloseCurrentPopup();
    }
    if (ImGui::BeginMenu("Primitive")) {
      for (int i = 0; i < IM_ARRAYSIZE(primitives); ++i)
        if (ImGui::Selectable(primitives[i])) {
          auto scene = GetActiveScene();
          scene->GetSpawnManager().Spawn(primitives[i]);
          ImGui::CloseCurrentPopup();
        }
      ImGui::EndMenu();
    }
    ImGui::EndPopup();
  }
}
