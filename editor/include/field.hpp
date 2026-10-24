#pragma once
#include <algorithm>
#include <cfloat>
#include <cstring>
#include <imgui.h>
#include <string>
#include <utility>
/// @brief The inspector's labelled widgets, with the label ending at the panel's right edge.
///
/// Dear ImGui writes a widget's label immediately to the right of its field, starting where the
/// field stops, and has no setting for doing otherwise. So each of these gives the widget a label
/// it will not draw and writes the text itself: in the same strip to the right of the field, at the
/// same height, but placed by where it ends rather than where it begins. Icons are part of the
/// label and go with it.
///
/// Written straight into the window's draw list rather than as an item. An item would take part in
/// the layout -- it would have to be put on the widget's line, and whatever followed on that line
/// would then follow the label -- and the point is that nothing but the text moves. The cost is
/// that the label is no longer something to click.
///
/// Each has the name and the arguments of the widget it stands in for, so a call is changed by
/// changing `ImGui::` to `field::` and nothing else. A row that puts two labelled widgets side by
/// side cannot use these for both, since both labels would be sent to the same place.
///
/// A yes-or-no is not one of these. It is a `Toggle`, further down, which keeps its label beside it.
namespace field {
/// @brief The label with `##` in front, which Dear ImGui takes as an identity and draws none of.
inline auto Hidden(const char *label) -> const char * {
  static thread_local std::string id;
  id.assign("##").append(label);
  return id.c_str();
}
/// @brief Writes a label so that it ends at the right edge of what is left of the line.
///
/// Called before the widget, while the cursor is still where the widget is about to go. `width` is
/// how wide the widget's own part will be. A label too long for the strip is started at the strip's
/// left instead, as it always was, and runs off the edge as it always did.
inline auto Label(const char *label, const float width) -> void {
  // Up to a `##`, which is where a label that carries an identity of its own stops being text.
  const auto *hash = std::strstr(label, "##");
  const auto *end = hash ? hash : label + std::strlen(label);
  if (end == label)
    return;
  const auto &style = ImGui::GetStyle();
  const auto origin = ImGui::GetCursorScreenPos();
  const auto right = origin.x + ImGui::GetContentRegionAvail().x;
  const auto strip = origin.x + width + style.ItemInnerSpacing.x;
  const ImVec2 at(std::max(strip, right - ImGui::CalcTextSize(label, end).x), origin.y + style.FramePadding.y);
  ImGui::GetWindowDrawList()->AddText(at, ImGui::GetColorU32(ImGuiCol_Text), label, end);
}
#define KUKI_FIELD(Name) \
  template <typename... A> \
  inline auto Name(const char *label, A &&...args) { \
    Label(label, ImGui::CalcItemWidth()); \
    return ImGui::Name(Hidden(label), std::forward<A>(args)...); \
  }
KUKI_FIELD(BeginCombo)
KUKI_FIELD(ColorEdit3)
KUKI_FIELD(ColorEdit4)
KUKI_FIELD(Combo)
KUKI_FIELD(DragFloat)
KUKI_FIELD(DragFloat2)
KUKI_FIELD(DragFloat3)
KUKI_FIELD(DragInt)
KUKI_FIELD(InputFloat3)
KUKI_FIELD(InputInt)
KUKI_FIELD(InputScalar)
KUKI_FIELD(InputText)
KUKI_FIELD(LabelText)
KUKI_FIELD(SliderFloat)
KUKI_FIELD(SliderInt)
#undef KUKI_FIELD
/// @brief A switch that is a button: its icon and its name, lit while what it stands for is on.
///
/// What a checkbox is everywhere here. The icon and the label are one button, so the whole of it
/// is the thing to press -- as the box and its label together were for a checkbox, and as the icon
/// alone was not when the label was only text beside it.
///
/// It is a button like any other and takes the style's padding as it finds it. All that marks it as
/// on is the colour, which is the one a button has while it is held down, so that on looks pressed
/// in. Off, the icon and the label are dimmed as well, since the two background colours alone are
/// close enough to be mistaken at a glance.
///
/// The label is also the identity, as a checkbox's is, so two in one window need different labels
/// or a `##` to tell them apart; the part after a `##` is not drawn. Answers whether it was pressed.
inline auto Toggle(const char *icon, const char *label, bool *value) -> bool {
  const auto *hash = std::strstr(label, "##");
  const std::string text(label, hash ? hash : label + std::strlen(label));
  ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(*value ? ImGuiCol_ButtonActive : ImGuiCol_Button));
  ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(*value ? ImGuiCol_Text : ImGuiCol_TextDisabled));
  const auto pressed = ImGui::Button((std::string(icon) + (text.empty() ? "" : " ") + text + "###" + label).c_str());
  ImGui::PopStyleColor(2);
  if (pressed)
    *value = !*value;
  return pressed;
}
/// @brief A title to press, with what it heads shown or hidden under it: every fold in the editor.
///
/// What `CollapsingHeader` and `TreeNode` are with the arrow taken off. Neither can be drawn
/// without one short of being made a leaf, and a leaf does not fold; so this is a button in the
/// header's colours, with its text at the left, that keeps whether it is open in the window's own
/// state storage -- which is where those widgets keep it too, so a section stays as it was left for
/// as long as its window is.
///
/// `fill` makes it the width of the panel, which is what heads a section. Without it the button is
/// only as wide as its title, for a fold that shares its line with something else. Nothing is
/// indented under either: what follows is laid out as it would have been with no fold there.
///
/// It is the last item when it returns, so a context menu can still be hung on it.
inline auto Fold(const char *label, const bool openByDefault = false, const bool fill = true) -> bool {
  auto *storage = ImGui::GetStateStorage();
  const auto id = ImGui::GetID(label);
  auto open = storage->GetBool(id, openByDefault);
  ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_Header));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetStyleColorVec4(ImGuiCol_HeaderHovered));
  ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImGui::GetStyleColorVec4(ImGuiCol_HeaderActive));
  ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(.0f, .5f));
  if (ImGui::Button(label, ImVec2(fill ? -FLT_MIN : .0f, .0f))) {
    open = !open;
    storage->SetBool(id, open);
  }
  ImGui::PopStyleVar();
  ImGui::PopStyleColor(3);
  return open;
}
/// @brief Puts the next item on the current line, ending at the line's right edge.
///
/// The other half of a row that reads label first: the label is written where the line starts and
/// whatever it names is sent here, so that a list of such rows has its names down one side and its
/// values down the other. `width` is how wide the item is going to be. One too wide for what is
/// left of the line simply follows the label.
inline auto RightAlign(const float width) -> void {
  ImGui::SameLine();
  ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(.0f, ImGui::GetContentRegionAvail().x - width));
}
/// @brief A row of two pieces of text: what it is on the left, and the answer against the right edge.
inline auto Row(const char *label, const char *value) -> void {
  ImGui::TextDisabled("%s", label);
  RightAlign(ImGui::CalcTextSize(value).x);
  ImGui::TextUnformatted(value);
}
} // namespace field
