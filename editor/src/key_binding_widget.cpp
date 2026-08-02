#include <algorithm>
#include <application.hpp>
#include <imgui.h>
#include <input_manager.hpp>
#include <key_binding_widget.hpp>
using namespace kuki;
namespace {
auto TruncateText(const std::string &text, float maxWidth) -> std::string {
  if (ImGui::CalcTextSize(text.c_str()).x <= maxWidth)
    return text;
  static constexpr auto ellipsis = "...";
  const auto ellipsisWidth = ImGui::CalcTextSize(ellipsis).x;
  if (maxWidth <= ellipsisWidth)
    return ellipsis;
  auto truncated = text;
  while (!truncated.empty() && ImGui::CalcTextSize((truncated + ellipsis).c_str()).x > maxWidth)
    truncated.pop_back();
  return truncated + ellipsis;
}
} // namespace
auto DisplayKeyBindingRow(Application &app, const char *label, const std::string &description, const std::string &bindingName, int &rebindingIndex, int index) -> void {
  ImGui::PushID(index);
  ImGui::TextUnformatted(label);
  if (!description.empty() && ImGui::IsItemHovered())
    ImGui::SetTooltip("%s", description.c_str());
  ImGui::SameLine(140.f);
  const auto rebinding = rebindingIndex == index;
  if (rebinding)
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(.8f, .2f, .2f, 1.f));
  const auto buttonLabel = rebinding ? "Press a key..." : app.GetTriggerName(app.GetBinding(bindingName));
  if (ImGui::Button(buttonLabel.c_str(), ImVec2(140.f, 0.f)) && rebindingIndex == -1) {
    rebindingIndex = index;
    app.BeginKeyCapture();
    app.SetInputCaptureActive(true);
  }
  if (rebinding)
    ImGui::PopStyleColor();
  ImGui::PopID();
  if (rebinding) {
    const auto outcome = app.PollKeyCapture();
    if (outcome.state == InputManager::CaptureState::Captured)
      app.SetBinding(bindingName, outcome.trigger);
    if (outcome.state != InputManager::CaptureState::Pending) {
      rebindingIndex = -1;
      app.SetInputCaptureActive(false);
    }
  }
}
auto DisplaySequenceRow(const std::string &sequence, const std::string &description) -> void {
  const auto sequenceWidth = ImGui::CalcTextSize(sequence.c_str()).x + ImGui::GetStyle().FramePadding.x * 2.f;
  const auto avail = ImGui::GetContentRegionAvail().x;
  const auto descriptionMaxWidth = std::max(0.f, avail - sequenceWidth - ImGui::GetStyle().ItemSpacing.x);
  ImGui::TextUnformatted(TruncateText(description, descriptionMaxWidth).c_str());
  if (!description.empty() && ImGui::IsItemHovered())
    ImGui::SetTooltip("%s", description.c_str());
  ImGui::SameLine();
  ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - sequenceWidth);
  ImGui::BeginDisabled();
  ImGui::Button(sequence.c_str());
  ImGui::EndDisabled();
}
