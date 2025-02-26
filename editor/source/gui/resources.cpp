#include <application.hpp>
#include <asset_loader.hpp>
#include <component/material.hpp>
#include <component/mesh.hpp>
#include <component/transform.hpp>
#include <editor.hpp>
#include <entity_manager.hpp>
#include <filesystem>
#include <imgui.h>
#include <list>
#include <render_system.hpp>
#include <unordered_map>
#include <utility>
void Editor::DisplayResources() {
  static const auto THUMBNAIL_SIZE = 96.0f;
  static const auto TILE_SIZE = ImVec2(THUMBNAIL_SIZE, THUMBNAIL_SIZE);
  ImGui::Begin("Resources");
  if (ImGui::BeginPopupContextWindow()) {
    if (ImGui::BeginMenu("Import")) {
      if (ImGui::Selectable("Model"))
        fileBrowser.Open();
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
    auto tilesPerRow = static_cast<int>(contentRegion.x / TILE_SIZE.x);
    auto tileCount = 0;
    if (tilesPerRow > 0)
      assetManager.ForAll([&](unsigned int assetID) {
        if (assetManager.HasComponents<Transform, Mesh, Material>(assetID)) {
          ImVec2 tilePos((tileCount % tilesPerRow) * TILE_SIZE.x, (tileCount / tilesPerRow) * TILE_SIZE.y);
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
            renderSystem->RenderAssetToTexture(assetID, textureID, TILE_SIZE.x);
          ImGui::Image(textureID, TILE_SIZE);
          tileCount++;
        }
      });
  }
  updateThumbnails = false;
  ImGui::End();
  fileBrowser.Display();
  if (fileBrowser.HasSelected()) {
    auto filepath = fileBrowser.GetSelected();
    auto filename = filepath.stem().string();
    assetLoader.LoadModel(filename, filepath);
    fileBrowser.ClearSelected();
  }
}
