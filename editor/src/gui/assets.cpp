#include <application.hpp>
#include <editor.hpp>
#include <imgui.h>
#include <render_system.hpp>
#include <string>
#include <utility>
void Editor::DisplayAssets() {
  static const ImVec2 uv0(.0f, 1.0f);
  static const ImVec2 uv1(1.0f, .0f);
  static const auto THUMBNAIL_SIZE = 96.0f;
  static const ImVec2 TILE_SIZE(THUMBNAIL_SIZE, THUMBNAIL_SIZE);
  auto renderSystem = GetSystem<RenderSystem>();
  if (!renderSystem)
    return;
  ImGui::Begin("Assets");
  if (ImGui::BeginPopupContextWindow()) {
    if (ImGui::BeginMenu("Import")) {
      if (ImGui::Selectable("Model"))
        fileBrowser.Open();
      ImGui::EndMenu();
    }
    ImGui::EndPopup();
  }
  const auto contentRegion = ImGui::GetContentRegionAvail();
  const auto tilesPerRow = std::max(1, static_cast<int>(contentRegion.x / THUMBNAIL_SIZE));
  const auto scrollY = ImGui::GetScrollY();
  const auto visibleHeight = ImGui::GetWindowHeight();
  struct AssetInfo {
    unsigned int id;
    ImVec2 position;
  };
  auto tileCount = 0;
  ForEachRootAsset([&](unsigned int assetID) {
    ImVec2 tilePos((tileCount % tilesPerRow) * THUMBNAIL_SIZE, (tileCount / tilesPerRow) * THUMBNAIL_SIZE);
    auto tileTop = tilePos.y;
    auto tileBottom = tilePos.y + THUMBNAIL_SIZE;
    if (tileBottom >= scrollY && tileTop <= scrollY + visibleHeight) {
      ImGui::SetCursorPos(tilePos);
      ImGui::PushID(assetID);
      auto textureID = renderSystem->RenderAssetToTexture(assetID, THUMBNAIL_SIZE);
      auto assetName = GetAssetName(assetID);
      if (ImGui::ImageButton(std::to_string(textureID).c_str(), textureID, TILE_SIZE, uv0, uv1)) {}
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
    tileCount++;
  });
  ImGui::End();
  fileBrowser.Display();
  if (fileBrowser.HasSelected()) {
    auto filepath = fileBrowser.GetSelected();
    LoadModel(filepath);
    fileBrowser.ClearSelected();
  }
}
