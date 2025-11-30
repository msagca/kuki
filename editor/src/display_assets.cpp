#include <algorithm>
#include <application.hpp>
#include <component.hpp>
#include <component_traits.hpp>
#include <cstdint>
#include <editor.hpp>
#include <filesystem>
#include <id.hpp>
#include <imgui.h>
#include <material.hpp>
#include <rendering_system.hpp>
#include <skybox.hpp>
#include <spdlog/spdlog.h>
#include <string>
#include <texture.hpp>
#include <variant>
#include <vector>
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
  static const auto TEXT_PADDING = 2.0f;
  static const auto TEXT_HEIGHT = ImGui::GetTextLineHeight();
  static const auto TILE_TOTAL_WIDTH = THUMBNAIL_SIZE + TILE_PADDING;
  static const auto TILE_TOTAL_HEIGHT = THUMBNAIL_SIZE + TEXT_PADDING + TEXT_HEIGHT + TILE_PADDING;
  static auto fileType = FileType::None;
  auto renderSystem = GetSystem<RenderingSystem>();
  if (!renderSystem)
    return;
  ImGui::Begin("Assets");
  const auto contentRegion = ImGui::GetContentRegionAvail();
  const auto tilesPerRow = std::max(1, static_cast<int>(contentRegion.x / TILE_TOTAL_WIDTH));
  const auto scrollY = ImGui::GetScrollY();
  const auto visibleHeight = ImGui::GetWindowHeight();
  auto cursorStartPos = ImGui::GetCursorPos();
  if (ImGui::BeginPopupContextWindow("ImportMenu", ImGuiPopupFlags_NoOpenOverItems | ImGuiPopupFlags_MouseButtonRight)) {
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
  std::vector<ID> assetIds;
  if (context.assetMask == -1)
    ForEachRootAsset([&assetIds](ID assetId) { assetIds.push_back(assetId); });
  else if ((context.assetMask & (static_cast<int>(ComponentMask::Texture) | static_cast<int>(ComponentMask::Skybox))) != 0) {
    ForEachAsset<Texture>([&assetIds](ID assetId, Texture* _) { assetIds.push_back(assetId); });
    ForEachAsset<Skybox>([&assetIds](ID assetId, Skybox* _) { assetIds.push_back(assetId); });
  }
  const auto totalRows = (assetIds.size() + tilesPerRow - 1) / tilesPerRow;
  const auto totalContentHeight = totalRows * TILE_TOTAL_HEIGHT;
  ImGui::Dummy(ImVec2(0, totalContentHeight));
  ImGui::SetCursorPos(cursorStartPos);
  for (auto i = 0; i < assetIds.size(); ++i) {
    const auto& id = assetIds[i];
    const auto row = i / tilesPerRow;
    const auto col = i % tilesPerRow;
    ImVec2 tilePos(cursorStartPos.x + col * TILE_TOTAL_WIDTH, cursorStartPos.y + row * TILE_TOTAL_HEIGHT);
    auto tileTop = tilePos.y;
    auto tileBottom = tilePos.y + TILE_TOTAL_HEIGHT;
    if (tileBottom >= scrollY && tileTop <= scrollY + visibleHeight) {
      ImGui::SetCursorPos(tilePos);
      ImGui::PushID(i);
      auto textureId = renderSystem->RenderAssetToTexture(id, THUMBNAIL_SIZE);
      auto assetName = GetAssetName(id);
      ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(.0f, .0f));
      if (ImGui::ImageButton(std::to_string(textureId).c_str(), textureId, TILE_SIZE, uv0, uv1)) {
        // HACK: the following needs to be handled by a dedicated method
        if (context.selectedEntity.IsValid() && context.selectedComponent >= 0) {
          auto component = GetEntityComponent(context.selectedEntity, static_cast<ComponentType>(context.selectedComponent));
          if (component) {
            auto [assetTexture, assetSkybox] = GetAssetComponents<Texture, Skybox>(id);
            if (auto entityMaterial = component->As<Material>()) {
              if (auto entityLitMaterial = std::get_if<LitMaterial>(&entityMaterial->material)) {
                if (assetTexture)
                  switch (context.selectedProperty) {
                  case static_cast<int>(MaterialProperty::AlbedoTexture):
                    entityLitMaterial->data.albedo = assetTexture->id;
                    break;
                  }
              }
            } else if (auto entitySkybox = component->As<Skybox>())
              if (assetSkybox)
                *entitySkybox = *assetSkybox;
          }
        }
        context.selectedComponent = -1;
        context.selectedProperty = -1;
      }
      if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
        ImGui::SetDragDropPayload("SPAWN_ASSET", assetName.c_str(), (assetName.size() + 1) * sizeof(char));
        ImGui::EndDragDropSource();
      }
      ImGui::PopStyleVar();
      auto textWidth = ImGui::CalcTextSize(assetName.c_str()).x;
      auto maxTextWidth = THUMBNAIL_SIZE;
      auto textX = tilePos.x + (THUMBNAIL_SIZE - std::min(textWidth, maxTextWidth)) * 0.5f;
      ImGui::SetCursorPos(ImVec2(textX, tilePos.y + THUMBNAIL_SIZE + TEXT_PADDING));
      if (textWidth > maxTextWidth) {
        ImGui::PushTextWrapPos(tilePos.x + THUMBNAIL_SIZE);
        ImGui::TextWrapped("%s", assetName.c_str());
        ImGui::PopTextWrapPos();
      } else
        ImGui::Text("%s", assetName.c_str());
      ImGui::PopID();
    }
  }
  ImGui::End();
  fileBrowser.Display();
  if (fileBrowser.HasSelected()) {
    auto filepath = fileBrowser.GetSelected();
    if (!std::filesystem::exists(filepath))
      spdlog::error("File does not exist: '{}'.", filepath.string());
    else if (fileType == FileType::Model)
      LoadModelAsync(filepath);
    else if (fileType == FileType::Image)
      LoadTextureAsync(filepath, TextureType::HDR);
    fileBrowser.ClearSelected();
    fileType = FileType::None;
  }
}
