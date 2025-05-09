#include <application.hpp>
#include <component/component.hpp>
#include <component/skybox.hpp>
#include <component/texture.hpp>
#include <cstdint>
#include <editor.hpp>
#include <imgui.h>
#include <string>
#include <system/rendering.hpp>
#include <utility>
#include <vector>
#include <glm/ext/vector_uint3.hpp>
using namespace kuki;
enum class FileType : uint8_t {
  None,
  Model,
  Image
};
void Editor::DisplayAssets() {
  static const ImVec2 uv0(.0f, 1.0f);
  static const ImVec2 uv1(1.0f, .0f);
  static const auto THUMBNAIL_SIZE = 128.0f;
  static const ImVec2 TILE_SIZE(THUMBNAIL_SIZE, THUMBNAIL_SIZE);
  static const auto TILE_PADDING = 2.0f;
  static const auto TILE_TOTAL_SIZE = THUMBNAIL_SIZE + TILE_PADDING;
  static auto fileType = FileType::None;
  auto renderSystem = GetSystem<RenderingSystem>();
  if (!renderSystem)
    return;
  ImGui::Begin("Assets");
  const auto contentRegion = ImGui::GetContentRegionAvail();
  const auto tilesPerRow = std::max(1, static_cast<int>(contentRegion.x / TILE_TOTAL_SIZE));
  const auto scrollY = ImGui::GetScrollY();
  const auto visibleHeight = ImGui::GetWindowHeight();
  auto cursorStartPos = ImGui::GetCursorPos();
  if (ImGui::BeginPopupContextWindow()) {
    if (ImGui::BeginMenu("Import")) {
      if (ImGui::Selectable("Image")) {
        fileType = FileType::Image;
        fileBrowser.SetTypeFilters({".exr", ".hdr"});
        fileBrowser.Open();
      }
      if (ImGui::Selectable("Model")) {
        fileType = FileType::Model;
        fileBrowser.SetTypeFilters({".gltf"});
        fileBrowser.Open();
      }
      ImGui::EndMenu();
    }
    ImGui::EndPopup();
  }
  auto tileCount = 0;
  std::vector<unsigned int> assetIds;
  if (assetMask == -1)
    ForEachRootAsset([&assetIds](unsigned int assetId) { assetIds.push_back(assetId); });
  else if ((assetMask & (static_cast<int>(ComponentMask::Texture) | static_cast<int>(ComponentMask::Skybox))) != 0) {
    ForEachAsset<Texture>([&assetIds](unsigned int assetId, Texture* _) { assetIds.push_back(assetId); });
    ForEachAsset<Skybox>([&assetIds](unsigned int assetId, Skybox* _) { assetIds.push_back(assetId); });
  }
  for (const auto id : assetIds) {
    ImVec2 tilePos(cursorStartPos.x + (tileCount % tilesPerRow) * TILE_TOTAL_SIZE, cursorStartPos.y + (tileCount / tilesPerRow) * TILE_TOTAL_SIZE);
    auto tileTop = tilePos.y;
    auto tileBottom = tilePos.y + THUMBNAIL_SIZE;
    if (tileBottom >= scrollY && tileTop <= scrollY + visibleHeight) {
      ImGui::SetCursorPos(tilePos);
      ImGui::PushID(id);
      auto textureId = renderSystem->RenderAssetToTexture(id, THUMBNAIL_SIZE);
      auto assetName = GetAssetName(id);
      ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(.0f, .0f));
      if (ImGui::ImageButton(std::to_string(textureId).c_str(), textureId, TILE_SIZE, uv0, uv1)) {
        if (selectedEntity >= 0) {
          auto component = GetEntityComponent(selectedEntity, selectedComponentName);
          if (component) {
            auto [textureComp, skyboxComp] = GetAssetComponents<Texture, Skybox>(id);
            if (skyboxComp)
              selectedProperty.value = glm::uvec3{skyboxComp->id};
            else if (textureComp)
              selectedProperty.value = int{textureComp->id};
            if (skyboxComp || textureComp)
              component->SetProperty(selectedProperty);
          }
        }
        selectedComponentName = "";
      }
      if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
        ImGui::SetDragDropPayload("SPAWN_ASSET", assetName.c_str(), (assetName.size() + 1) * sizeof(char));
        ImGui::EndDragDropSource();
      }
      ImGui::PopStyleVar();
      auto textWidth = ImGui::CalcTextSize(assetName.c_str()).x;
      auto textX = tilePos.x + (THUMBNAIL_SIZE - textWidth) * .5f;
      ImGui::SetCursorPos(ImVec2(textX, tilePos.y + THUMBNAIL_SIZE + 2.0f));
      ImGui::Text("%s", assetName.c_str());
      ImGui::PopID();
    }
    tileCount++;
  }
  ImGui::End();
  fileBrowser.Display();
  if (fileBrowser.HasSelected()) {
    auto filepath = fileBrowser.GetSelected();
    if (fileType == FileType::Model)
      LoadModelAsync(filepath);
    else if (fileType == FileType::Image)
      LoadTextureAsync(filepath, TextureType::HDR);
    fileBrowser.ClearSelected();
    fileType = FileType::None;
  }
}
