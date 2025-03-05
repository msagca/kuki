#include <application.hpp>
#include <asset_loader.hpp>
#include <editor.hpp>
#include <entity_manager.hpp>
#include <imgui.h>
#include <list>
#include <render_system.hpp>
#include <string>
#include <unordered_map>
#include <vector>
void Editor::DisplayAssets() {
  static const auto THUMBNAIL_SIZE = 96.0f;
  static const auto CLEANUP_PERIOD = 5.0f;
  static const auto CACHE_TIMEOUT = 30.0f;
  static const ImVec2 TILE_SIZE(THUMBNAIL_SIZE, THUMBNAIL_SIZE);
  ImGui::Begin("Assets");
  if (ImGui::BeginPopupContextWindow()) {
    if (ImGui::BeginMenu("Import")) {
      if (ImGui::Selectable("Model"))
        fileBrowser.Open();
      ImGui::EndMenu();
    }
    ImGui::EndPopup();
  }
  auto renderSystem = GetSystem<RenderSystem>();
  if (!renderSystem) {
    ImGui::Text("Render system unavailable");
    ImGui::End();
    return;
  }
  static std::unordered_map<unsigned int, size_t> assetToTexture;
  static std::unordered_map<unsigned int, float> lastUsedTime;
  if (updateThumbnails) {
    std::vector<unsigned int> assetsToUpdate;
    assetManager.ForEachRoot([&assetsToUpdate](unsigned int assetID) {
      assetsToUpdate.push_back(assetID);
    });
    for (auto it = assetToTexture.begin(); it != assetToTexture.end();)
      if (std::find(assetsToUpdate.begin(), assetsToUpdate.end(), it->first) == assetsToUpdate.end()) {
        texturePool.Release(it->second);
        it = assetToTexture.erase(it);
        lastUsedTime.erase(it->first);
      } else
        ++it;
  }
  const auto contentRegion = ImGui::GetContentRegionAvail();
  const auto tilesPerRow = std::max(1, static_cast<int>(contentRegion.x / TILE_SIZE.x));
  const auto scrollY = ImGui::GetScrollY();
  const auto visibleHeight = ImGui::GetWindowHeight();
  struct AssetInfo {
    unsigned int id;
    ImVec2 position;
  };
  std::vector<AssetInfo> visibleAssets;
  auto tileCount = 0;
  assetManager.ForEachRoot([&](unsigned int assetID) {
    ImVec2 tilePos((tileCount % tilesPerRow) * TILE_SIZE.x, (tileCount / tilesPerRow) * TILE_SIZE.y);
    auto tileTop = tilePos.y;
    auto tileBottom = tilePos.y + TILE_SIZE.y;
    if (tileBottom >= scrollY && tileTop <= scrollY + visibleHeight)
      visibleAssets.push_back({assetID, tilePos});
    tileCount++;
  });
  auto contentHeight = ((tileCount + tilesPerRow - 1) / tilesPerRow) * TILE_SIZE.y;
  ImGui::SetCursorPosY(contentHeight);
  ImGui::Dummy(ImVec2(1.0f, 1.0f));
  ImGui::SetCursorPos(ImVec2(.0f, .0f));
  const auto currentTime = ImGui::GetTime();
  for (const auto& asset : visibleAssets) {
    auto assetID = asset.id;
    auto tilePos = asset.position;
    ImGui::SetCursorPos(tilePos);
    ImGui::PushID(assetID);
    auto textureID = 0;
    auto it = assetToTexture.find(assetID);
    if (it == assetToTexture.end()) {
      auto itemID = texturePool.Request();
      assetToTexture[assetID] = itemID;
      textureID = *texturePool.Get(itemID);
      renderSystem->RenderAssetToTexture(assetID, textureID, TILE_SIZE.x);
    } else {
      textureID = *texturePool.Get(it->second);
      if (updateThumbnails)
        renderSystem->RenderAssetToTexture(assetID, textureID, TILE_SIZE.x);
    }
    lastUsedTime[assetID] = currentTime;
    auto& assetName = assetManager.GetName(assetID);
    if (ImGui::ImageButton(std::to_string(textureID).c_str(), textureID, TILE_SIZE, ImVec2(.0f, .0f), ImVec2(1.0f, 1.0f))) {}
    //SelectAsset(assetID);
    if (ImGui::IsItemHovered()) {
      ImGui::BeginTooltip();
      ImGui::Text("%s", assetName.c_str());
      ImGui::EndTooltip();
    }
    if (ImGui::BeginDragDropSource()) {
      ImGui::SetDragDropPayload("ASSET_ID", &assetID, sizeof(unsigned int));
      ImGui::Image(textureID, TILE_SIZE);
      ImGui::EndDragDropSource();
    }
    ImGui::PopID();
  }
  static auto lastCleanupTime = .0f;
  if (currentTime - lastCleanupTime > CLEANUP_PERIOD) {
    for (auto it = lastUsedTime.begin(); it != lastUsedTime.end();)
      if (currentTime - it->second > CACHE_TIMEOUT) {
        auto textureIt = assetToTexture.find(it->first);
        if (textureIt != assetToTexture.end()) {
          texturePool.Release(textureIt->second);
          assetToTexture.erase(textureIt);
        }
        it = lastUsedTime.erase(it);
      } else
        ++it;
    lastCleanupTime = currentTime;
  }
  updateThumbnails = false;
  ImGui::End();
  fileBrowser.Display();
  if (fileBrowser.HasSelected()) {
    auto filepath = fileBrowser.GetSelected();
    assetLoader.LoadModel(filepath);
    fileBrowser.ClearSelected();
    updateThumbnails = true;
  }
}
