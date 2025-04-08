#include <application.hpp>
#include <editor.hpp>
#include <imgui.h>
#include <render_system.hpp>
#include <spdlog/spdlog.h>
#include <string>
void Editor::DisplayAssets() {
  static const ImVec2 uv0(.0f, 1.0f);
  static const ImVec2 uv1(1.0f, .0f);
  static const auto THUMBNAIL_SIZE = 128.0f;
  static const ImVec2 TILE_SIZE(THUMBNAIL_SIZE, THUMBNAIL_SIZE);
  static const auto TILE_PADDING = 2.0f;
  static const auto TILE_TOTAL_SIZE = THUMBNAIL_SIZE + TILE_PADDING;
  auto renderSystem = GetSystem<RenderSystem>();
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
      if (ImGui::Selectable("Model"))
        fileBrowser.Open();
      ImGui::EndMenu();
    }
    ImGui::EndPopup();
  }
  auto tileCount = 0;
  ForEachRootAsset([&](unsigned int assetId) {
    ImVec2 tilePos(cursorStartPos.x + (tileCount % tilesPerRow) * TILE_TOTAL_SIZE, cursorStartPos.y + (tileCount / tilesPerRow) * TILE_TOTAL_SIZE);
    auto tileTop = tilePos.y;
    auto tileBottom = tilePos.y + THUMBNAIL_SIZE;
    if (tileBottom >= scrollY && tileTop <= scrollY + visibleHeight) {
      ImGui::SetCursorPos(tilePos);
      ImGui::PushID(assetId);
      // TODO: no need to render all assets in every frame, call this only if a new asset has been added
      auto textureId = renderSystem->RenderAssetToTexture(assetId, THUMBNAIL_SIZE);
      auto assetName = GetAssetName(assetId);
      ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(.0f, .0f));
      if (ImGui::ImageButton(std::to_string(textureId).c_str(), textureId, TILE_SIZE, uv0, uv1)) {
        // TODO: implement selection logic here
      }
      if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
        ImGui::SetDragDropPayload("SPAWN_ASSET", assetName.c_str(), (assetName.size() + 1) * sizeof(char));
        ImGui::Text("%s", assetName.c_str());
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
  });
  ImGui::End();
  fileBrowser.Display();
  if (fileBrowser.HasSelected()) {
    auto filepath = fileBrowser.GetSelected();
    LoadModel(filepath);
    spdlog::info("Loaded model file '{}'.", filepath.string());
    fileBrowser.ClearSelected();
  }
}
