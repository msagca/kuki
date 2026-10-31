#pragma once
#include <field.hpp>
#include <icons_lucide.hpp>
#include <algorithm>
#include <animator.hpp>
#include <anti_aliasing.hpp>
#include <atmosphere_sky.hpp>
#include <application.hpp>
#include <array>
#include <bounding_box.hpp>
#include <camera.hpp>
#include <cctype>
#include <cmath>
#include <component_type.hpp>
#include <dx_material.hpp>
#include <editor.hpp>
#include <enum_traits.hpp>
#include <exposure.hpp>
#include <gl_buffer.hpp>
#include <gl_compute_shader.hpp>
#include <gl_lit_shader.hpp>
#include <gl_material.hpp>
#include <gl_mesh.hpp>
#include <gl_render_target.hpp>
#include <gl_skybox.hpp>
#include <gl_texture.hpp>
#include <gl_unlit_shader.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>
#include <indirect_lighting.hpp>
#include <light.hpp>
#include <material_fallback.hpp>
#include <material_type.hpp>
#include <mesh_handle.hpp>
#include <model_asset.hpp>
#include <particle_presets.hpp>
#include <render_binding.hpp>
#include <skeleton.hpp>
#include <skybox_handle.hpp>
#include <string>
#include <planar_reflection.hpp>
#include <stylization.hpp>
#include <texture_handle.hpp>
struct PropertyDisplayer {
  EditorContext &context;
  Editor &app;
  template <typename T>
  auto operator()(T *) -> void;
};
/// @brief The half of a material that is the same whichever backend is holding it.
///
/// `GLMaterial` and `DXMaterial` differ in how they reach their textures and in nothing else: the
/// surface values underneath are one `MaterialFallback` in both. Shared rather than ported, because
/// two copies of a panel this long is how the two backends end up offering different materials.
///
/// `toon` offers the cel shading slider, which only a backend that draws the bands should.
inline auto DisplayMaterialSurface(kuki::MaterialType &type, kuki::MaterialFallback &fallback, const bool toon = false) -> void {
  static auto &types = kuki::EnumTraits<kuki::MaterialType>::GetNames();
  auto typeIndex = static_cast<int>(type);
  if (field::Combo("Type", &typeIndex, types.data(), types.size()))
    type = static_cast<kuki::MaterialType>(typeIndex);
  auto albedoColor = fallback.albedo;
  if (field::ColorEdit4(ICON_LC_PALETTE " Albedo Color", glm::value_ptr(albedoColor)))
    fallback.albedo = albedoColor;
  auto specularColor = fallback.specular;
  if (field::ColorEdit4(ICON_LC_PALETTE " Specular Color", glm::value_ptr(specularColor)))
    fallback.specular = specularColor;
  auto emissiveColor = fallback.emissive;
  if (field::ColorEdit4(ICON_LC_PALETTE " Emissive Color", glm::value_ptr(emissiveColor)))
    fallback.emissive = emissiveColor;
  auto metalnessFactor = fallback.metalness;
  if (field::SliderFloat("Metalness Factor", &metalnessFactor, .0f, 1.f))
    fallback.metalness = metalnessFactor;
  auto occlusionFactor = fallback.occlusion;
  if (field::SliderFloat("Occlusion Factor", &occlusionFactor, .0f, 1.f))
    fallback.occlusion = occlusionFactor;
  auto roughnessFactor = fallback.roughness;
  if (field::SliderFloat("Roughness Factor", &roughnessFactor, .0f, 1.f))
    fallback.roughness = roughnessFactor;
  static auto &alphaModes = kuki::EnumTraits<kuki::AlphaMode>::GetNames();
  auto alphaMode = static_cast<int>(fallback.alphaMode);
  if (field::Combo("Alpha Mode", &alphaMode, alphaModes.data(), alphaModes.size()))
    fallback.alphaMode = static_cast<kuki::AlphaMode>(alphaMode);
  if (fallback.alphaMode == kuki::AlphaMode::Mask) {
    auto alphaCutoff = fallback.alphaCutoff;
    if (field::SliderFloat("Alpha Cutoff", &alphaCutoff, .0f, 1.f))
      fallback.alphaCutoff = alphaCutoff;
  }
  auto ior = fallback.ior;
  if (field::SliderFloat("Index of Refraction", &ior, 1.f, 3.f))
    fallback.ior = ior;
  ImGui::SetItemTooltip("Also fixes how reflective the surface is head on, so it matters to every\nmaterial rather than only the transmissive ones. 1.5 is the usual 0.04.");
  auto transmission = fallback.transmission;
  if (field::SliderFloat("Transmission", &transmission, .0f, 1.f))
    fallback.transmission = transmission;
  if (toon) {
    auto amount = fallback.toon;
    if (field::SliderFloat("Toon", &amount, .0f, 1.f))
      fallback.toon = amount;
    ImGui::SetItemTooltip("How far the surface is drawn in the scene's cel bands rather than shaded\nphysically. The bands themselves are the scene's Stylization.");
  }
  if (fallback.transmission > .0f) {
    auto thickness = fallback.thickness;
    if (field::SliderFloat("Thickness", &thickness, .0f, 10.f))
      fallback.thickness = thickness;
    auto attenuationColor = glm::vec3(fallback.attenuation);
    if (field::ColorEdit3(ICON_LC_PALETTE " Attenuation Color", glm::value_ptr(attenuationColor)))
      fallback.attenuation = glm::vec4(attenuationColor, fallback.attenuation.w);
    auto attenuationDistance = fallback.attenuation.w;
    if (field::SliderFloat("Attenuation Distance", &attenuationDistance, .0f, 10.f))
      fallback.attenuation.w = attenuationDistance;
  }
}
template <typename T>
inline auto PropertyDisplayer::operator()(T *) -> void {}
/// @brief The states of an animator, to be added, changed, tried and removed.
///
/// Everything `AnimationState` holds, a row each. Where the value is one of a known few it is a
/// list to choose from rather than a name to type: the model a clip comes from is one of the models
/// that are loaded and have clips, the clip is one of that model's, the root is one of the
/// skeleton's nodes and what comes next is one of the other states. A name typed wrong is a state
/// that silently plays nothing, and none of these can be.
///
/// Changing where a clip comes from forgets what was worked out about the old one -- its index and
/// which bones its channels move -- so that the animation system works it out again for the new.
///
/// Trying a state enters it and sets the animator playing. Stopped, that is a preview stepped by
/// the animation system for the purpose; nothing is saved of which state was being looked at.
inline auto DisplayAnimatorStates(Editor &app, kuki::Animator &animator, const kuki::ModelAsset &model) -> void {
  static constexpr auto NAME_LENGTH = 64;
  if (!field::Fold(ICON_LC_WORKFLOW " States", true))
    return;
  const auto Forget = [](kuki::AnimationState &state) {
    state.clipIndex = -1;
    state.nodes.clear();
  };
  if (field::BeginCombo(ICON_LC_FOOTPRINTS " Root Node", animator.rootNode.empty() ? "(none)" : animator.rootNode.c_str())) {
    if (ImGui::Selectable("(none)", animator.rootNode.empty()))
      animator.rootNode.clear();
    for (const auto &node : model.nodes)
      if (!node.name.empty() && ImGui::Selectable(node.name.c_str(), node.name == animator.rootNode))
        animator.rootNode = node.name;
    ImGui::EndCombo();
  }
  ImGui::SetItemTooltip("The node whose travel along the ground is taken out of the pose and handed to the\ngame, so a walk moves the entity instead of striding off and snapping back.");
  // The models a clip could come from: every one that is loaded and has any, but for this one,
  // which is offered first under a name that says so.
  std::vector<const kuki::ModelAsset *> sources;
  app.ForEachAsset(kuki::AssetType::Model, [&](const kuki::Asset *asset) {
    if (const auto *candidate = asset->As<kuki::ModelAsset>(); candidate && candidate != &model && !candidate->animations.empty())
      sources.push_back(candidate);
  });
  auto removed = -1;
  for (auto i = 0; i < static_cast<int>(animator.states.size()); ++i) {
    auto &state = animator.states[i];
    ImGui::PushID(i);
    // Named for the state and identified by its place, so that the fold does not shut every time
    // a letter is typed into the name.
    const auto current = animator.state == i;
    if (field::Fold(((current ? ICON_LC_PLAY " " : "") + (state.name.empty() ? std::string("(unnamed)") : state.name) + "###State").c_str())) {
      char name[NAME_LENGTH]{};
      state.name.copy(name, NAME_LENGTH - 1);
      if (field::InputText("Name", name, NAME_LENGTH))
        state.name = name;
      // A blend takes the place of the one clip, so the rows that choose that clip are greyed out
      // for as long as there is anything in it.
      ImGui::BeginDisabled(!state.blend.empty());
      const auto *from = &model;
      for (const auto *source : sources)
        if (source->id == state.source)
          from = source;
      const auto fromName = from == &model ? std::string("(this model)") : app.GetAssetName(from->id);
      if (field::BeginCombo(ICON_LC_BOX " Clip From", state.source && from == &model ? "(not loaded)" : fromName.c_str())) {
        if (ImGui::Selectable("(this model)", !state.source)) {
          state.source = {};
          state.clip.clear();
          Forget(state);
        }
        for (const auto *source : sources) {
          ImGui::PushID(source);
          if (ImGui::Selectable(app.GetAssetName(source->id).c_str(), source->id == state.source)) {
            state.source = source->id;
            state.clip.clear();
            Forget(state);
          }
          ImGui::PopID();
        }
        ImGui::EndCombo();
      }
      if (field::BeginCombo(ICON_LC_FILM " Clip", state.clip.empty() ? "(first)" : state.clip.c_str())) {
        if (ImGui::Selectable("(first)", state.clip.empty())) {
          state.clip.clear();
          Forget(state);
        }
        for (size_t clip = 0; clip < from->animations.size(); ++clip) {
          ImGui::PushID(static_cast<int>(clip));
          if (ImGui::Selectable(from->animations[clip].name.c_str(), from->animations[clip].name == state.clip)) {
            state.clip = from->animations[clip].name;
            Forget(state);
          }
          ImGui::PopID();
        }
        ImGui::EndCombo();
      }
      ImGui::EndDisabled();
      // Each clip of the blend is which model it is from and the value it stands at, on one line
      // with the button that removes it. The first clip of the model is the one taken, which is
      // how a clip that came in a file of its own is named.
      auto removedBlend = -1;
      for (auto mixed = 0; mixed < static_cast<int>(state.blend.size()); ++mixed) {
        auto &entry = state.blend[mixed];
        ImGui::PushID(1000 + mixed);
        const auto entryName = entry.source ? app.GetAssetName(entry.source) : std::string("(this model)");
        ImGui::SetNextItemWidth(ImGui::CalcItemWidth() * .65f);
        if (ImGui::BeginCombo("##From", entryName.empty() ? "(not loaded)" : entryName.c_str())) {
          for (const auto *source : sources) {
            ImGui::PushID(source);
            if (ImGui::Selectable(app.GetAssetName(source->id).c_str(), source->id == entry.source)) {
              entry.source = source->id;
              entry.clip.clear();
              entry.clipIndex = -1;
              entry.nodes.clear();
            }
            ImGui::PopID();
          }
          ImGui::EndCombo();
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(ImGui::CalcItemWidth() * .35f - ImGui::GetStyle().ItemSpacing.x);
        ImGui::DragFloat("##At", &entry.at, .05f, -1000.f, 1000.f, "at %.2f");
        ImGui::SameLine();
        if (ImGui::Button(ICON_LC_TRASH_2))
          removedBlend = mixed;
        ImGui::PopID();
      }
      if (removedBlend >= 0)
        state.blend.erase(state.blend.begin() + removedBlend);
      if (ImGui::Button(ICON_LC_BLEND " Add Blend Clip"))
        state.blend.push_back({.at = state.blend.empty() ? .0f : state.blend.back().at + 1.f});
      if (state.blend.size() > 1) {
        auto low = state.blend.front().at;
        auto high = low;
        for (const auto &entry : state.blend) {
          low = std::min(low, entry.at);
          high = std::max(high, entry.at);
        }
        field::SliderFloat(ICON_LC_BLEND " Blend", &state.value, low, high, "%.2f");
        ImGui::SetItemTooltip("Where along its blend the state is. A game sets this as it runs; here it is\nfor trying the blend out.");
      }
      field::Toggle(ICON_LC_REPEAT, "Loop", &state.loop);
      field::DragFloat(ICON_LC_GAUGE " Speed", &state.speed, .01f, .05f, 10.f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
      field::DragInt("Fade", &state.fade, .2f, 0, 600, "%d ticks", ImGuiSliderFlags_AlwaysClamp);
      ImGui::SetItemTooltip("How many ticks the pose takes to arrive from whatever was showing when this\nstate was entered.");
      ImGui::BeginDisabled(state.loop);
      if (field::BeginCombo("Next", state.next.empty() ? "(hold the last pose)" : state.next.c_str())) {
        if (ImGui::Selectable("(hold the last pose)", state.next.empty()))
          state.next.clear();
        for (auto other = 0; other < static_cast<int>(animator.states.size()); ++other) {
          ImGui::PushID(other);
          if (other != i && !animator.states[other].name.empty() && ImGui::Selectable(animator.states[other].name.c_str(), animator.states[other].name == state.next))
            state.next = animator.states[other].name;
          ImGui::PopID();
        }
        ImGui::EndCombo();
      }
      ImGui::EndDisabled();
      // An event is a tick and a name on one line, with the button that removes it at the end. Two
      // fields to a row, so neither is one of the labelled ones: the row is its own label.
      auto removedEvent = -1;
      for (auto event = 0; event < static_cast<int>(state.events.size()); ++event) {
        ImGui::PushID(event);
        ImGui::SetNextItemWidth(ImGui::CalcItemWidth() * .35f);
        ImGui::DragInt("##Tick", &state.events[event].tick, .2f, 0, 100000, "tick %d", ImGuiSliderFlags_AlwaysClamp);
        ImGui::SameLine();
        char eventName[NAME_LENGTH]{};
        state.events[event].name.copy(eventName, NAME_LENGTH - 1);
        ImGui::SetNextItemWidth(ImGui::CalcItemWidth() * .65f - ImGui::GetStyle().ItemSpacing.x);
        if (ImGui::InputTextWithHint("##Event", "event name", eventName, NAME_LENGTH))
          state.events[event].name = eventName;
        ImGui::SameLine();
        if (ImGui::Button(ICON_LC_TRASH_2))
          removedEvent = event;
        ImGui::PopID();
      }
      if (removedEvent >= 0)
        state.events.erase(state.events.begin() + removedEvent);
      if (ImGui::Button(ICON_LC_FLAG " Add Event"))
        state.events.push_back({0, "event"});
      ImGui::SameLine();
      if (ImGui::Button(ICON_LC_PLAY " Try")) {
        animator.Play(state.name, true);
        animator.playing = true;
      }
      ImGui::SameLine();
      if (ImGui::Button(ICON_LC_TRASH_2 " Remove"))
        removed = i;
    }
    ImGui::PopID();
  }
  if (removed >= 0) {
    animator.states.erase(animator.states.begin() + removed);
    animator.state = -1;
    animator.stateTicks = 0;
  }
  if (ImGui::Button(ICON_LC_PLUS " Add State")) {
    kuki::AnimationState state;
    state.name = "state" + std::to_string(animator.states.size() + 1);
    state.loop = true;
    animator.states.push_back(std::move(state));
  }
}
/// @brief The entity a goal reaches for, chosen from a list or dropped on from the hierarchy.
///
/// Listed as the hierarchy has them, a step in for each level, since one character's bones are all
/// called what the other's are and only where they hang tells them apart. Searched, the list is
/// only what matches, each with the name of the entity at the top of its tree beside it for the
/// same reason.
inline auto PickEntity(Editor &app, const char *label, kuki::EntityID &entity) -> void {
  static constexpr auto FIND_LENGTH = 64;
  static char find[FIND_LENGTH]{};
  const auto name = entity ? app.GetEntityName(entity) : std::string("(none)");
  if (field::BeginCombo(label, name.c_str(), ImGuiComboFlags_HeightLarge)) {
    if (ImGui::IsWindowAppearing())
      ImGui::SetKeyboardFocusHere();
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::InputTextWithHint("##Find", ICON_LC_SEARCH " find", find, FIND_LENGTH);
    std::string wanted(find);
    std::ranges::transform(wanted, wanted.begin(), [](const unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (wanted.empty() && ImGui::Selectable("(none)", !entity))
      entity = kuki::EntityID::Invalid;
    const auto Walk = [&](const kuki::EntityID id, const int depth, const std::string &top, const auto &self) -> void {
      auto entityName = app.GetEntityName(id);
      auto lowered = entityName;
      std::ranges::transform(lowered, lowered.begin(), [](const unsigned char c) { return static_cast<char>(std::tolower(c)); });
      if (wanted.empty() || lowered.find(wanted) != std::string::npos) {
        ImGui::PushID(static_cast<int>(id));
        const auto shown = wanted.empty() ? std::string(static_cast<size_t>(depth) * 2, ' ') + entityName : entityName + (top.empty() || top == entityName ? "" : "  (" + top + ")");
        if (ImGui::Selectable(shown.c_str(), id == entity))
          entity = id;
        ImGui::PopID();
      }
      app.ForEachChildEntity(id, [&](const kuki::EntityID child) {
        self(child, depth + 1, top.empty() ? entityName : top, self);
      });
    };
    app.ForEachRootEntity([&](const kuki::EntityID id) {
      Walk(id, 0, std::string{}, Walk);
    });
    ImGui::EndCombo();
  }
  if (ImGui::BeginDragDropTarget()) {
    if (const auto *payload = ImGui::AcceptDragDropPayload("##EntityPayload"); payload && payload->DataSize == sizeof(kuki::EntityID))
      entity = *static_cast<const kuki::EntityID *>(payload->Data);
    ImGui::EndDragDropTarget();
  }
}
/// @brief An animator's inverse kinematics goals, to be added, changed and removed.
///
/// The rows that mean something only to one kind of goal are shown for that kind alone. The bone is
/// one of the model's nodes rather than a name to type, for the same reason a state's clip is: a
/// name typed wrong is a goal that silently does nothing.
inline auto DisplayAnimatorIK(Editor &app, kuki::Animator &animator, const kuki::ModelAsset &model) -> void {
  static constexpr auto NAME_LENGTH = 64;
  if (!field::Fold(ICON_LC_HAND_GRAB " Inverse Kinematics"))
    return;
  static auto &kinds = kuki::EnumTraits<kuki::IKKind>::GetNames();
  auto removed = -1;
  for (auto i = 0; i < static_cast<int>(animator.ik.size()); ++i) {
    auto &goal = animator.ik[i];
    ImGui::PushID(i);
    const auto aim = goal.kind == kuki::IKKind::Aim;
    const auto title = !goal.name.empty() ? goal.name : !goal.bone.empty() ? goal.bone : std::string("(no bone)");
    if (field::Fold(((aim ? ICON_LC_EYE " " : ICON_LC_HAND_GRAB " ") + title + "###Goal").c_str(), true)) {
      char name[NAME_LENGTH]{};
      goal.name.copy(name, NAME_LENGTH - 1);
      if (field::InputText(ICON_LC_TAG " Name", name, NAME_LENGTH))
        goal.name = name;
      ImGui::SetItemTooltip("What the game finds the goal by.");
      auto kind = static_cast<int>(goal.kind);
      if (field::Combo("Kind", &kind, kinds.data(), static_cast<int>(kinds.size())))
        goal.kind = static_cast<kuki::IKKind>(kind);
      ImGui::SetItemTooltip("Reach bends the bone's parent and grandparent to put the bone on the point.\nAim turns the bone, and the parents sharing the turn, to face it.");
      if (field::BeginCombo(ICON_LC_BONE " Bone", goal.bone.empty() ? "(none)" : goal.bone.c_str(), ImGuiComboFlags_HeightLarge)) {
        for (size_t node = 0; node < model.nodes.size(); ++node) {
          ImGui::PushID(static_cast<int>(node));
          if (!model.nodes[node].name.empty() && ImGui::Selectable(model.nodes[node].name.c_str(), model.nodes[node].name == goal.bone))
            goal.bone = model.nodes[node].name;
          ImGui::PopID();
        }
        ImGui::EndCombo();
      }
      PickEntity(app, ICON_LC_CROSSHAIR " Target", goal.target);
      ImGui::SetItemTooltip("The entity the point goes with. None leaves the point fixed in the world.\nAn entity can be dragged here from the hierarchy.");
      field::DragFloat3(ICON_LC_MOVE_3D " Offset", &goal.offset.x, .01f);
      ImGui::SetItemTooltip(goal.target ? "Where the point is in the target's own space." : "Where the point is in the world.");
      field::SliderFloat(ICON_LC_WEIGHT " Weight", &goal.weight, .0f, 1.f, "%.2f");
      if (aim) {
        field::DragFloat3(ICON_LC_AXIS_3D " Axis", &goal.axis.x, .01f, -1.f, 1.f, "%.2f");
        ImGui::SetItemTooltip("The way that is to face the point, in the character's own axes as the model\nwas built. Forward is 0, 0, 1 for most.");
        field::DragInt(ICON_LC_LINK " Chain", &goal.chain, .05f, 1, 16, "%d bones", ImGuiSliderFlags_AlwaysClamp);
        ImGui::SetItemTooltip("How many bones share the turn, counting up from this one.");
        field::SliderFloat(ICON_LC_ANGLE " Limit", &goal.limit, .0f, 180.f, "%.0f deg");
      } else {
        field::DragFloat3(ICON_LC_SPLINE " Bend", &goal.bend.x, .01f, -1.f, 1.f, "%.2f");
        ImGui::SetItemTooltip("The way the middle joint points, in the character's own axes: forward for a\nknee. Zero keeps the way the clip bends it.");
        field::Toggle(ICON_LC_ANCHOR, "Hold Rotation", &goal.holdRotation);
        ImGui::SetItemTooltip("Keep the bone turned in the world as the pose had it, as a foot set flat\nstays flat, rather than turning with the bone above.");
        ImGui::SameLine();
      }
      if (ImGui::Button(ICON_LC_TRASH_2 " Remove"))
        removed = i;
    }
    ImGui::PopID();
  }
  if (removed >= 0)
    animator.ik.erase(animator.ik.begin() + removed);
  if (ImGui::Button(ICON_LC_PLUS " Add Goal"))
    animator.ik.push_back({});
}
/// @brief An animator's clips: its states, or with none the one clip it plays and where it is in it.
inline auto DisplayAnimatorClips(Editor &app, kuki::Animator *animator) -> void {
  const auto modelName = app.GetAssetName(animator->modelAssetId);
  field::LabelText(ICON_LC_BOX " Model", "%s", modelName.empty() ? "(none)" : modelName.c_str());
  // The clip list comes from the model, so everything below it depends on that model being loaded.
  // Without it there is nothing to name the clips with and nothing to bound the scrub by, so the
  // raw index is offered on its own rather than a set of controls that cannot describe themselves.
  const auto *model = app.GetAsset<kuki::ModelAsset>(animator->modelAssetId);
  if (model) {
    DisplayAnimatorStates(app, *animator, *model);
    // With states the animator is a machine and not a clip being scrubbed: which clip is playing
    // and how far in are the machine's business, so all that is offered is whether it runs.
    if (!animator->states.empty()) {
      field::Toggle(ICON_LC_PLAY, "Playing", &animator->playing);
      return;
    }
  }
  if (!model || model->animations.empty()) {
    auto clipIndex = animator->clipIndex;
    if (field::InputInt(ICON_LC_FILM " Clip", &clipIndex))
      animator->clipIndex = clipIndex;
    return;
  }
  const auto clipCount = static_cast<int>(model->animations.size());
  const auto valid = animator->clipIndex >= 0 && animator->clipIndex < clipCount;
  const auto *preview = valid ? model->animations[animator->clipIndex].name.c_str() : "(none)";
  if (field::BeginCombo(ICON_LC_FILM " Clip", preview)) {
    if (ImGui::Selectable("(none)", !valid))
      animator->clipIndex = -1;
    for (auto i = 0; i < clipCount; ++i) {
      ImGui::PushID(i);
      if (ImGui::Selectable(model->animations[i].name.c_str(), animator->clipIndex == i)) {
        animator->clipIndex = i;
        animator->time = .0f;
      }
      ImGui::PopID();
    }
    ImGui::EndCombo();
  }
  if (!valid)
    return;
  const auto &clip = model->animations[animator->clipIndex];
  auto playing = animator->playing;
  if (field::Toggle(ICON_LC_PLAY, "Playing", &playing))
    animator->playing = playing;
  ImGui::SameLine();
  auto loop = animator->loop;
  if (field::Toggle(ICON_LC_REPEAT, "Loop", &loop))
    animator->loop = loop;
  // Scrubbed in ticks because that is what the animation system advances and what the clip's
  // duration is measured in; the seconds underneath are the part a person can judge, and are only
  // meaningful while the clip declares a rate to convert by.
  auto time = animator->time;
  if (field::SliderFloat(ICON_LC_CLOCK " Time", &time, .0f, clip.duration, "%.1f ticks", ImGuiSliderFlags_AlwaysClamp))
    animator->time = time;
}
template <>
inline auto PropertyDisplayer::operator()<kuki::Animator>(kuki::Animator *animator) -> void {
  if (!animator)
    return;
  DisplayAnimatorClips(app, animator);
  if (const auto *model = app.GetAsset<kuki::ModelAsset>(animator->modelAssetId); model)
    DisplayAnimatorIK(app, *animator, *model);
}
template <>
inline auto PropertyDisplayer::operator()<kuki::BoundingBox>(kuki::BoundingBox *bounds) -> void {
  if (!bounds)
    return;
  auto boundsMin = bounds->min;
  field::InputFloat3("Min Bounds", glm::value_ptr(boundsMin), nullptr, ImGuiInputTextFlags_ReadOnly);
  auto boundsMax = bounds->max;
  field::InputFloat3("Max Bounds", glm::value_ptr(boundsMax), nullptr, ImGuiInputTextFlags_ReadOnly);
}
template <>
inline auto PropertyDisplayer::operator()<kuki::Camera>(kuki::Camera *camera) -> void {
  static constexpr auto MAX_FLOAT = std::numeric_limits<float>::max();
  if (!camera)
    return;
  auto dirty = false;
  // First, because it decides whether anything else here is what you are looking at. A scene may
  // hold several cameras and only one of them draws; editing the others is legitimate and invisible,
  // and without this there is nothing on the panel that says which case you are in.
  const auto entity = context.selectedEntityId;
  const auto active = app.GetCamera() == camera;
  ImGui::BeginDisabled(active);
  auto makeActive = active;
  if (field::Toggle(ICON_LC_VIDEO, "Active", &makeActive) && makeActive)
    app.SetActiveCamera(entity);
  ImGui::EndDisabled();
  ImGui::SetItemTooltip("Whether this is the camera the scene is drawn through.\nDisabled when it already is: a scene always has one, so the way to\nchange it is to activate another rather than to deactivate this.");
  ImGui::Separator();
  static auto &types = kuki::EnumTraits<kuki::CameraType>::GetNames();
  auto type = static_cast<int>(camera->type);
  if (field::Combo("Type", &type, types.data(), types.size())) {
    const auto newType = static_cast<kuki::CameraType>(type);
    if (newType == kuki::CameraType::Orthographic && camera->type != kuki::CameraType::Orthographic) {
      const auto distance = glm::length(camera->position);
      if (distance > .0f)
        camera->orthoSize = distance * glm::tan(glm::radians(camera->fov) * .5f);
    }
    camera->type = newType;
    dirty = true;
  }
  auto position = camera->position;
  if (field::DragFloat3(ICON_LC_MOVE_3D " Position", glm::value_ptr(position), .1f)) {
    camera->position = position;
    dirty = true;
  }
  auto rotationDegrees = glm::degrees(glm::eulerAngles(camera->rotation));
  if (field::DragFloat3(ICON_LC_ROTATE_3D " Rotation", glm::value_ptr(rotationDegrees), .1f)) {
    camera->rotation = glm::quat(glm::radians(rotationDegrees));
    dirty = true;
  }
  if (camera->type == kuki::CameraType::Perspective) {
    auto fov = camera->fov;
    if (field::SliderFloat("FOV", &fov, .0f, 180.f)) {
      camera->fov = fov;
      dirty = true;
    }
    // Shown, not offered. This follows the render target, and the control that used to sit here
    // took a drag and silently reverted on the next frame because nothing read the result back.
    ImGui::BeginDisabled();
    auto aspectRatio = camera->aspectRatio;
    field::DragFloat("Aspect Ratio", &aspectRatio);
    ImGui::EndDisabled();
    ImGui::SetItemTooltip("Follows the size of the viewport this camera renders into.");
  }
  auto nearPlane = camera->nearPlane;
  if (field::DragFloat("Near Plane", &nearPlane, .1f, .0f, MAX_FLOAT)) {
    camera->nearPlane = nearPlane;
    dirty = true;
  }
  auto farPlane = camera->farPlane;
  if (field::DragFloat("Far Plane", &farPlane, .1f, .0f, MAX_FLOAT)) {
    camera->farPlane = farPlane;
    dirty = true;
  }
  if (camera->type == kuki::CameraType::Orthographic) {
    auto orthoSize = camera->orthoSize;
    if (field::DragFloat("Size", &orthoSize, .1f, .0f, MAX_FLOAT)) {
      camera->orthoSize = orthoSize;
      dirty = true;
    }
  }
  static auto &modes = kuki::EnumTraits<kuki::ExposureMode>::GetNames();
  auto mode = static_cast<int>(camera->exposureMode);
  if (field::Combo("Exposure Mode", &mode, modes.data(), modes.size()))
    camera->exposureMode = static_cast<kuki::ExposureMode>(mode);
  ImGui::SetItemTooltip("Manual sets the scale directly, in stops.\nPhysical derives it from an aperture, shutter and sensitivity, the way a camera does.");
  if (camera->exposureMode == kuki::ExposureMode::Physical) {
    // Written the way the dials on a camera are labelled rather than the way the formula wants
    // them. Aperture is an f-number, so a step towards the wide end halves it and doubles the
    // light; shutter is a reciprocal, since nobody reads 0.008 as a hundred and twenty fifth.
    auto aperture = camera->aperture;
    if (field::DragFloat("Aperture", &aperture, .05f, kuki::MIN_APERTURE, kuki::MAX_APERTURE, "f/%.1f", ImGuiSliderFlags_AlwaysClamp))
      camera->aperture = aperture;
    ImGui::SetItemTooltip("Lower opens the lens and lets in more light.");
    auto denominator = 1.f / std::clamp(camera->shutterSpeed, kuki::MIN_SHUTTER_SPEED, kuki::MAX_SHUTTER_SPEED);
    if (field::DragFloat("Shutter", &denominator, 1.f, 1.f / kuki::MAX_SHUTTER_SPEED, 1.f / kuki::MIN_SHUTTER_SPEED, "1/%.0f s", ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_Logarithmic))
      camera->shutterSpeed = 1.f / std::max(denominator, 1.f / kuki::MAX_SHUTTER_SPEED);
    ImGui::SetItemTooltip("Lower holds the shutter open longer and lets in more light.");
    auto sensitivity = camera->sensitivity;
    if (field::DragFloat("ISO", &sensitivity, 10.f, kuki::MIN_SENSITIVITY, kuki::MAX_SENSITIVITY, "%.0f", ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_Logarithmic))
      camera->sensitivity = sensitivity;
    ImGui::SetItemTooltip("Higher amplifies what the sensor gathered.");
  }
  auto exposure = camera->exposure;
  const auto compensating = camera->exposureMode == kuki::ExposureMode::Physical;
  if (field::SliderFloat(compensating ? "Compensation" : "Exposure", &exposure, -kuki::EXPOSURE_RANGE, kuki::EXPOSURE_RANGE, "%+.2f EV", ImGuiSliderFlags_AlwaysClamp))
    camera->exposure = exposure;
  ImGui::SetItemTooltip("Stops applied on top of whatever the mode produced. One stop doubles the light.");
  ImGui::Separator();
  // Kept to the bottom of the panel and behind a fold, under everything that describes the camera
  // itself. These do not change what the camera is, only what it draws instead of the picture, and
  // somebody opening this panel is far more often here for the lens than for the diagnostics.
  // The whole fold goes when the backend answers neither, which on the OpenGL one is both of them:
  // its scene shader writes finished pixels and nothing else, and there is no probe field for the
  // spheres to stand for. An empty fold would still read as a feature that had stopped working.
  if ((context.capabilities.lightingDebugViews || context.capabilities.probeVolume) && field::Fold("Debug Views")) {
    if (context.capabilities.lightingDebugViews) {
      static auto &lightingNames = kuki::EnumTraits<kuki::LightingDebugView>::GetNames();
      auto lighting = static_cast<int>(camera->lightingDebugView);
      if (field::Combo("Shading", &lighting, lightingNames.data(), lightingNames.size()))
        camera->lightingDebugView = static_cast<kuki::LightingDebugView>(lighting);
      ImGui::SetItemTooltip("One step of the shading, drawn instead of the finished pixel.\nThe bounce, the sky, the direct light and the occlusion are terms;\nthe probe entries are how the field was reconstructed at each point.\nShown without exposure or a tone curve, so a value can be read off the screen.");
    }
    if (context.capabilities.probeVolume) {
      static auto &probeNames = kuki::EnumTraits<kuki::ProbeDebugView>::GetNames();
      auto probe = static_cast<int>(camera->probeDebugView);
      if (field::Combo("Probes", &probe, probeNames.data(), probeNames.size()))
        camera->probeDebugView = static_cast<kuki::ProbeDebugView>(probe);
      ImGui::SetItemTooltip("What the probes are drawn as, over the scene.\nIrradiance is what the field holds; distance and variance are what the\nvisibility test runs on; trust, relocation and classification are\nwhether the mechanisms that maintain the field are working.");
    }
  }
  camera->dirty += dirty;
}
template <>
inline auto PropertyDisplayer::operator()<kuki::DXMaterial>(kuki::DXMaterial *material) -> void {
  if (!material)
    return;
  // No texture tiles here, unlike the OpenGL panel. Direct3D reaches its textures through a bindless
  // table rather than a handle per slot, so there is no per-texture id to hand Dear ImGui and
  // nothing to draw a thumbnail from. What the table holds is described instead of shown.
  auto tableIndex = static_cast<int>(material->textureTableIndex);
  field::InputInt("Texture Table", &tableIndex, 0, 0, ImGuiInputTextFlags_ReadOnly);
  ImGui::SetItemTooltip("Index of this material's descriptor table in the bindless heap.");
  ImGui::BeginDisabled();
  auto bound = material->textureTableGPU != 0;
  field::Toggle(ICON_LC_DATABASE, "Table Resident", &bound);
  ImGui::EndDisabled();
  ImGui::SetItemTooltip("Whether the table has been built and uploaded yet.");
  DisplayMaterialSurface(material->type, material->fallback, context.capabilities.stylization);
}
template <>
inline auto PropertyDisplayer::operator()<kuki::GLBuffer>(kuki::GLBuffer *buffer) -> void {
  if (!buffer)
    return;
  auto id = static_cast<int>(buffer->id);
  field::InputInt("Buffer Object", &id, 0, 0, ImGuiInputTextFlags_ReadOnly);
}
template <>
inline auto PropertyDisplayer::operator()<kuki::GLComputeShader>(kuki::GLComputeShader *shader) -> void {
  if (!shader)
    return;
  auto id = static_cast<int>(shader->id);
  field::InputInt("Program", &id, 0, 0, ImGuiInputTextFlags_ReadOnly);
}
template <>
inline auto PropertyDisplayer::operator()<kuki::GLLitShader>(kuki::GLLitShader *shader) -> void {
  if (!shader)
    return;
  auto id = static_cast<int>(shader->id);
  field::InputInt("Program", &id, 0, 0, ImGuiInputTextFlags_ReadOnly);
  static auto &types = kuki::EnumTraits<kuki::MaterialType>::GetNames();
  field::LabelText("Draws", "%s", types[static_cast<size_t>(shader->type)]);
}
template <>
inline auto PropertyDisplayer::operator()<kuki::GLMaterial>(kuki::GLMaterial *material) -> void {
  static constexpr ImVec2 TEXTURE_SIZE(64, 64);
  if (!material)
    return;
  const auto &style = ImGui::GetStyle();
  auto GetTileWidth = [&](const char *label) -> float {
    const auto textWidth = ImGui::CalcTextSize(label).x;
    const auto buttonWidth = TEXTURE_SIZE.x + style.FramePadding.x * 2.f;
    return (std::max)(buttonWidth, textWidth);
  };
  auto DrawTextureTile = [&](const char *id, ImTextureID tex, const char *label, bool pending, auto onClick) -> float {
    const auto tileWidth = GetTileWidth(label);
    auto startPos = ImGui::GetCursorPos();
    ImGui::BeginGroup();
    auto clicked = TextureButton(id, tex, TEXTURE_SIZE);
    if (pending)
      ImGui::GetWindowDrawList()->AddRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), PICKING_HIGHLIGHT_COLOR, 0.f, 0, PICKING_HIGHLIGHT_THICKNESS);
    if (clicked)
      onClick();
    const auto textWidth = ImGui::CalcTextSize(label).x;
    const auto buttonHeight = TEXTURE_SIZE.y + style.FramePadding.y * 2.f;
    ImGui::SetCursorPosX(startPos.x + (tileWidth - textWidth) * .5f);
    ImGui::SetCursorPosY(startPos.y + buttonHeight + style.ItemInnerSpacing.y);
    ImGui::TextUnformatted(label);
    ImGui::EndGroup();
    return tileWidth;
  };
  const auto SelectProperty = [&](const kuki::MaterialProperty prop) {
    context.state = EditorState::PickingAsset;
    context.pickingAssetType = AssetType::Texture;
    context.pickingEntityId = context.selectedEntityId;
    context.pickingProperty = prop;
    context.pickingTarget = PickingTarget::MaterialTexture;
  };
  struct TexEntry {
    const char *id;
    ImTextureID tex;
    const char *label;
    kuki::MaterialProperty prop;
  };
  std::array<TexEntry, 7> entries{
    TexEntry{"Albedo", static_cast<ImTextureID>(material->textures.albedo), "Albedo", kuki::MaterialProperty::AlbedoTexture},
    {"Normal", static_cast<ImTextureID>(material->textures.normal), "Normal", kuki::MaterialProperty::NormalTexture},
    {"Metalness", static_cast<ImTextureID>(material->textures.metalness), "Metalness", kuki::MaterialProperty::MetalnessTexture},
    {"Occlusion", static_cast<ImTextureID>(material->textures.occlusion), "Occlusion", kuki::MaterialProperty::OcclusionTexture},
    {"Roughness", static_cast<ImTextureID>(material->textures.roughness), "Roughness", kuki::MaterialProperty::RoughnessTexture},
    {"Specular", static_cast<ImTextureID>(material->textures.specular), "Specular", kuki::MaterialProperty::SpecularTexture},
    {"Emissive", static_cast<ImTextureID>(material->textures.emissive), "Emissive", kuki::MaterialProperty::EmissiveTexture},
  };
  auto firstOnLine = true;
  auto lineWidth = .0f;
  auto remaining = .0f;
  for (const auto &e : entries) {
    if (firstOnLine) {
      lineWidth = ImGui::GetContentRegionAvail().x;
      remaining = lineWidth;
    }
    const auto tileWidth = GetTileWidth(e.label);
    if (!firstOnLine) {
      if (tileWidth + style.ItemSpacing.x <= remaining) {
        ImGui::SameLine();
        remaining -= style.ItemSpacing.x;
      } else {
        firstOnLine = true;
        lineWidth = ImGui::GetContentRegionAvail().x;
        remaining = lineWidth;
      }
    }
    const auto pending = context.state == EditorState::PickingAsset && context.pickingEntityId == context.selectedEntityId && context.pickingProperty == e.prop;
    remaining -= DrawTextureTile(e.id, e.tex, e.label, pending, [&SelectProperty, &e] { SelectProperty(e.prop); });
    firstOnLine = false;
  }
  static auto &types = kuki::EnumTraits<kuki::MaterialType>::GetNames();
  DisplayMaterialSurface(material->type, material->fallback);
}
template <>
inline auto PropertyDisplayer::operator()<kuki::GLMesh>(kuki::GLMesh *mesh) -> void {
  if (!mesh)
    return;
  auto vao = static_cast<int>(mesh->vao);
  field::InputInt("Vertex Array Object", &vao, 1, 100, ImGuiInputTextFlags_ReadOnly);
  auto ebo = static_cast<int>(mesh->ebo);
  field::InputInt("Element Buffer Object", &ebo, 1, 100, ImGuiInputTextFlags_ReadOnly);
  auto vertexCount = mesh->vertexCount;
  field::InputInt("Vertex Count", &vertexCount, 1, 100, ImGuiInputTextFlags_ReadOnly);
  auto indexCount = mesh->indexCount;
  field::InputInt("Index Count", &indexCount, 1, 100, ImGuiInputTextFlags_ReadOnly);
}
template <>
inline auto PropertyDisplayer::operator()<kuki::GLRenderTarget>(kuki::GLRenderTarget *target) -> void {
  if (!target)
    return;
  field::LabelText("Size", "%d x %d", target->desc.width, target->desc.height);
  auto samples = target->desc.samples;
  field::InputInt("Samples", &samples, 0, 0, ImGuiInputTextFlags_ReadOnly);
  auto framebuffer = static_cast<int>(target->framebuffer);
  field::InputInt("Framebuffer", &framebuffer, 0, 0, ImGuiInputTextFlags_ReadOnly);
  auto renderbuffer = static_cast<int>(target->renderbuffer);
  field::InputInt("Renderbuffer", &renderbuffer, 0, 0, ImGuiInputTextFlags_ReadOnly);
  auto texture = static_cast<int>(target->texture);
  field::InputInt("Colour Texture", &texture, 0, 0, ImGuiInputTextFlags_ReadOnly);
  auto idTexture = static_cast<int>(target->idTexture);
  field::InputInt("Picking Texture", &idTexture, 0, 0, ImGuiInputTextFlags_ReadOnly);
  ImGui::SetItemTooltip("Entity ids written alongside the colour, for clicking things in the viewport.\nZero when this target was not asked for one.");
}
template <>
inline auto PropertyDisplayer::operator()<kuki::GLSkybox>(kuki::GLSkybox *skybox) -> void {
  static constexpr ImVec2 TEXTURE_SIZE(64, 64);
  if (!skybox)
    return;
  const auto &style = ImGui::GetStyle();
  const auto label = "Skybox";
  auto tex = ImTextureID{};
  auto uv0 = ImVec2(0.f, 1.f);
  auto uv1 = ImVec2(1.f, 0.f);
  if (auto *handle = app.GetEntityComponent<kuki::SkyboxHandle>(context.selectedEntityId); handle)
    if (auto *texture = app.PreviewAsset(handle->assetId); texture) {
      tex = static_cast<ImTextureID>(texture->GetTextureHandle());
      if (!texture->NeedsVerticalFlip()) {
        uv0 = ImVec2(0.f, 0.f);
        uv1 = ImVec2(1.f, 1.f);
      }
    }
  const float textWidth = ImGui::CalcTextSize(label).x;
  const float buttonWidth = TEXTURE_SIZE.x + style.FramePadding.x * 2.f;
  const float tileWidth = std::max(buttonWidth, textWidth);
  auto startPos = ImGui::GetCursorPos();
  ImGui::BeginGroup();
  const auto pending = context.state == EditorState::PickingAsset && context.pickingTarget == PickingTarget::Skybox && context.pickingEntityId == context.selectedEntityId;
  const auto clicked = TextureButton("Skybox##Texture", tex, TEXTURE_SIZE, uv0, uv1);
  if (pending)
    ImGui::GetWindowDrawList()->AddRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), PICKING_HIGHLIGHT_COLOR, 0.f, 0, PICKING_HIGHLIGHT_THICKNESS);
  if (clicked) {
    context.state = EditorState::PickingAsset;
    context.pickingAssetType = AssetType::Texture;
    context.pickingEntityId = context.selectedEntityId;
    context.pickingTarget = PickingTarget::Skybox;
  }
  const auto buttonHeight = TEXTURE_SIZE.y + style.FramePadding.y * 2.f;
  ImGui::SetCursorPosX(startPos.x + (tileWidth - textWidth) * .5f);
  ImGui::SetCursorPosY(startPos.y + buttonHeight + style.ItemInnerSpacing.y);
  ImGui::TextUnformatted(label);
  ImGui::EndGroup();
}
template <>
inline auto PropertyDisplayer::operator()<kuki::GLTexture>(kuki::GLTexture *texture) -> void {
  static constexpr auto MAX_INT = std::numeric_limits<int>::max();
  static constexpr auto TEXTURE_SIZE = ImVec2(64, 64);
  if (!texture)
    return;
  const auto textureId = static_cast<ImTextureID>(texture->id);
  if (TextureButton("ID", textureId, TEXTURE_SIZE)) {}
}
template <>
inline auto PropertyDisplayer::operator()<kuki::GLUnlitShader>(kuki::GLUnlitShader *shader) -> void {
  if (!shader)
    return;
  auto id = static_cast<int>(shader->id);
  field::InputInt("Program", &id, 0, 0, ImGuiInputTextFlags_ReadOnly);
  static auto &types = kuki::EnumTraits<kuki::MaterialType>::GetNames();
  field::LabelText("Draws", "%s", types[static_cast<size_t>(shader->type)]);
}
template <>
inline auto PropertyDisplayer::operator()<kuki::AntiAliasing>(kuki::AntiAliasing *antiAliasing) -> void {
  if (!antiAliasing)
    return;
  static auto &modes = kuki::EnumTraits<kuki::AntiAliasingMode>::GetNames();
  auto mode = static_cast<int>(antiAliasing->mode);
  if (field::Combo("Mode", &mode, modes.data(), modes.size()))
    antiAliasing->mode = static_cast<kuki::AntiAliasingMode>(mode);
  ImGui::SetItemTooltip("How many samples a pixel of the scene is rasterised at before it is\nresolved. Multisampling is the only method offered because it is the\nonly one the pipeline has: the antialiasing pass is a resolve, not a\nfilter with an opinion.");
}
template <>
inline auto PropertyDisplayer::operator()<kuki::AtmosphereSky>(kuki::AtmosphereSky *sky) -> void {
  if (!sky)
    return;
  // Said once and nothing else offered, on a backend with no route to a computed sky. The
  // add-component menu already hides this, but a scene file carries the component onto whatever
  // backend opens it -- and thirty sliders that move nothing read as a broken renderer.
  if (!context.capabilities.proceduralSky)
    return;
  // Edited through a copy and written back in one go, as every other displayer here does: a widget
  // writes on the frame it is dragged, and the renderer hashes this struct to decide whether to
  // spend twenty-one dispatches rebuilding the sky. Handing a widget the live field would let it
  // write while that hash is being taken.
  auto edited = *sky;
  if (field::Fold("Placement", true)) {
    field::SliderFloat("Altitude", &edited.viewAltitude, .0f, std::max(1.f, edited.atmosphereHeight), "%.2f km");
    ImGui::SetItemTooltip("How high above the ground the scene sits. At zero this is the sky from a\nfield; at a few kilometres the horizon drops, the blue deepens overhead\nand the haze thins out. Past the top of the atmosphere there is no air\nleft to scatter and the sky goes black.");
    field::SliderFloat("Sun Intensity", &edited.sunIntensity, .0f, 100.f, "%.1f");
    ImGui::SetItemTooltip("What the whole sky is multiplied by. Not a physical quantity and it cannot\nbe: this engine's lights carry bare multipliers rather than photometric\nunits, so there is no illuminance that would make the sky come out at a\ndefensible absolute brightness.");
    field::SliderFloat("Ground Albedo", &edited.groundAlbedo, .0f, 1.f);
    ImGui::SetItemTooltip("How much light the ground bounces back up. Most of the lower half of the\nsky cubemap is ground, and that cubemap is where the diffuse irradiance\nand the reflections are gathered from -- so this is most of the fill\nlight arriving from below, not a detail of the background.");
  }
  if (field::Fold("Sun Disc", true)) {
    field::SliderFloat("Angular Radius", &edited.sunAngularRadius, .0f, 5.f, "%.2f deg");
    ImGui::SetItemTooltip("A quarter of a degree is the real sun and looks it. Larger reads as a\nbigger, softer star, and costs nothing: the disc is a comparison against\na cosine rather than anything drawn.");
    field::SliderFloat("Disc Intensity", &edited.sunDiscIntensity, .0f, 4.f);
    ImGui::SetItemTooltip("A multiplier on the physical figure, which is the sun's whole output\narriving from seven millionths of a steradian -- correct, and more than\nthe bloom chain can take without whiting out the frame. Zero removes the\ndisc and leaves the sky it sits in alone.");
  }
  if (field::Fold("Air")) {
    field::DragFloat3("Rayleigh", &edited.rayleighScattering.x, 1.0e-4f, .0f, 1.f, "%.5f");
    ImGui::SetItemTooltip("Scattering per kilometre, per channel. The three differ by about a factor\nof six across the visible range, which is what scatters short wavelengths\nout of a sunbeam and into every other direction. Scaling all three makes\nthicker or thinner air; changing the ratio gives a sky that is not Earth's.");
    field::DragFloat("Rayleigh Scale Height", &edited.rayleighScaleHeight, .1f, .1f, 50.f, "%.2f km");
    ImGui::Separator();
    field::DragFloat("Mie Scattering", &edited.mieScattering, 1.0e-4f, .0f, 1.f, "%.5f");
    field::DragFloat("Mie Absorption", &edited.mieAbsorption, 1.0e-4f, .0f, 1.f, "%.5f");
    ImGui::SetItemTooltip("Added to the scattering to give the extinction, so a medium can never\nremove less light than it scatters. A negative value is clamped away.");
    field::DragFloat("Mie Scale Height", &edited.mieScaleHeight, .05f, .05f, 20.f, "%.2f km");
    field::SliderFloat("Mie Anisotropy", &edited.mieAnisotropy, .0f, .95f);
    ImGui::SetItemTooltip("How strongly the aerosols scatter forward. The knob to reach for when the\nsun looks like a sticker rather than a light: higher tightens the halo\nand brightens the sky right around the sun at the expense of the rest.");
    ImGui::Separator();
    field::DragFloat3("Ozone", &edited.ozoneAbsorption.x, 1.0e-5f, .0f, 1.f, "%.5f");
    ImGui::SetItemTooltip("Absorbs and does not scatter, and absorbs most in the middle of the\nvisible range -- which takes the green out of a low sun. Setting this to\nzero is the quickest way to see what it is doing.");
    field::DragFloat("Ozone Centre", &edited.ozoneCenter, .5f, .0f, 100.f, "%.1f km");
    field::DragFloat("Ozone Width", &edited.ozoneWidth, .5f, .1f, 100.f, "%.1f km");
  }
  if (field::Fold("Planet")) {
    field::DragFloat("Planet Radius", &edited.bottomRadius, 10.f, 100.f, 100000.f, "%.0f km");
    field::DragFloat("Atmosphere Height", &edited.atmosphereHeight, 1.f, 1.f, 1000.f, "%.0f km");
  }
  if (field::Fold("Quality")) {
    auto steps = static_cast<int>(edited.marchSteps);
    if (field::SliderInt("March Steps", &steps, 8, 128))
      edited.marchSteps = static_cast<uint32_t>(std::max(8, steps));
    ImGui::SetItemTooltip("Below about sixteen the banding shows near the horizon, where the medium\nchanges fastest along a ray.");
    field::SliderFloat("Multiple Scattering", &edited.multiscatterStrength, .0f, 2.f);
    ImGui::SetItemTooltip("One is the answer; this exists because the difference is otherwise hard\nto see and easy to mistake for something else. At zero the sky is single\nscattering only: too dark towards the horizon, with no blue hour at all\nonce the sun is down.");
  }
  if (edited != *sky)
    *sky = edited;
}
template <>
inline auto PropertyDisplayer::operator()<kuki::VolumetricClouds>(kuki::VolumetricClouds *clouds) -> void {
  if (!clouds)
    return;
  if (!context.capabilities.volumetrics)
    return;
  auto edited = *clouds;
  if (field::Fold("Layer", true)) {
    field::DragFloat("Base", &edited.bottomAltitude, .05f, .0f, 60.f, "%.2f km");
    ImGui::SetItemTooltip("Altitude of the cloud base above the ground. Compare it with the scene's own\naltitude on the Atmosphere Sky: a scene below this looks up at the layer, one\nabove it looks down on it, and one between the two is in the clouds.");
    field::DragFloat("Top", &edited.topAltitude, .05f, .05f, 80.f, "%.2f km");
    ImGui::SetItemTooltip("A thin layer reads as stratus however the shape is tuned, because there is no\nroom in it for the vertical development that makes a cumulus look like one.");
    field::SliderFloat("Coverage", &edited.coverage, .0f, 1.f);
    ImGui::SetItemTooltip("How much of the sky has cloud in it. A threshold rather than a multiplier, so\nraising it grows clouds together rather than thickening each one -- which is\nwhat cloud cover actually does.");
    field::DragFloat("Density", &edited.density, .25f, .0f, 80.f, "%.1f /km");
    ImGui::SetItemTooltip("Extinction of the cloud interior. This is what decides whether a cloud is a\nsolid white wall or something the sun comes through.");
  }
  if (field::Fold("Wind", true)) {
    field::DragFloat2("Direction", &edited.windDirection.x, .01f, -1.f, 1.f, "%.2f");
    field::DragFloat(ICON_LC_GAUGE " Speed", &edited.windSpeed, .001f, .0f, .2f, "%.3f km/s");
    ImGui::SetItemTooltip("Small numbers. A cloud field crossing a kilometre a minute is already brisk to\nlook at, and anything above about a tenth reads as weather on fast forward.");
  }
  if (field::Fold("Shape")) {
    field::DragFloat("Shape Scale", &edited.shapeScale, .1f, .1f, 60.f, "%.2f km");
    ImGui::SetItemTooltip("How big one cloud is. Raising it makes fewer, larger clouds out of the same\nfield rather than spreading the ones already there.");
    field::DragFloat("Detail Scale", &edited.detailScale, .01f, .01f, 5.f, "%.2f km");
    field::SliderFloat("Detail Strength", &edited.detailStrength, .0f, 1.f);
    ImGui::SetItemTooltip("How deeply the detail noise cuts into an edge, from smooth to shredded. It is\napplied to the boundary rather than the interior: the inside of a cloud is\nopaque, so detail spent there is detail thrown away.");
  }
  if (field::Fold("Light")) {
    field::SliderFloat("Anisotropy", &edited.anisotropy, .0f, .95f);
    ImGui::SetItemTooltip("How strongly the droplets scatter forward. High, because a droplet is enormous\ncompared to a wavelength -- this is what puts the silver lining on a cloud with\nthe sun behind it.");
    field::SliderFloat("Backscatter", &edited.backscatter, .0f, 1.f);
    ImGui::SetItemTooltip("How much of the backward lobe is mixed in. A cloud lit from behind has a bright\nrim and one lit from in front is bright all over, and a single lobe has to\nchoose between saying the first and saying the second.");
    field::SliderFloat("Powder", &edited.powder, .0f, 1.f);
    ImGui::SetItemTooltip("How dark the approach to an edge goes before it brightens. Light entering a thin\nedge has to scatter several times before it leaves and most of it does not,\nwhich is why a cumulus base reads as bruised rather than grey. Zero removes it.");
    field::SliderFloat("Ambient", &edited.ambient, .0f, 2.f);
    ImGui::SetItemTooltip("What the sky around a cloud fills its shaded side with, read from the same\ncubemap the scene's image-based lighting uses -- so a cloud at sunset is filled\nwith orange without that being said here.");
    field::SliderFloat("Scattering Scale", &edited.scatteringScale, .0f, 4.f);
    field::SliderFloat("Shadow Strength", &edited.shadowStrength, .0f, 1.f);
    ImGui::SetItemTooltip("How much the layer darkens the light reaching the air below it. What this\nreaches is the shafts in Volumetric Fog: a cloud crossing the sun takes the\nbeams with it.\n\nIt does not reach surface shading. The scene is shaded before this pass runs,\nso the ground under an overcast sky is still lit as though it were clear.");
  }
  if (field::Fold("Quality")) {
    auto steps = static_cast<int>(edited.steps);
    if (field::SliderInt("View Steps", &steps, 8, 192))
      edited.steps = static_cast<uint32_t>(std::max(8, steps));
    ImGui::SetItemTooltip("Distributed over the layer's thickness rather than the whole ray, so too few\nshows first as banding in the thick parts of a cloud.");
    auto lightSteps = static_cast<int>(edited.lightSteps);
    if (field::SliderInt("Light Steps", &lightSteps, 1, 16))
      edited.lightSteps = static_cast<uint32_t>(std::max(1, lightSteps));
    ImGui::SetItemTooltip("Taken from every view step, so this multiplies the count above. Six is enough\nbecause what it integrates is smooth and the several-orders term covers the\nrest.");
  }
  if (edited != *clouds)
    *clouds = edited;
}
template <>
inline auto PropertyDisplayer::operator()<kuki::Stylization>(kuki::Stylization *style) -> void {
  if (!style || !context.capabilities.stylization)
    return;
  auto edited = *style;
  if (field::Fold("Bands", true)) {
    auto bands = static_cast<int>(edited.bands);
    if (field::SliderInt("Bands", &bands, 1, 6))
      edited.bands = static_cast<uint32_t>(std::max(1, bands));
    ImGui::SetItemTooltip("Tones a lit surface is drawn in, for the materials whose Toon is above zero.");
    field::SliderFloat("Softness", &edited.bandSoftness, .0f, .2f);
    field::SliderFloat("Terminator", &edited.terminator, -.5f, .5f);
    ImGui::SetItemTooltip("Where the shaded side begins. Above zero pushes it towards the light, which\nmakes a form read rounder than its geometry.");
    field::ColorEdit3("Shadow Tint", &edited.shadowTint.x);
    field::SliderFloat("Highlight", &edited.highlightSize, .0f, .5f);
    field::SliderFloat("Rim", &edited.rimIntensity, .0f, 4.f);
    field::SliderFloat("Rim Width", &edited.rimWidth, .0f, 1.f);
  }
  if (field::Fold("Ink", true)) {
    field::SliderFloat("Thickness", &edited.inkThickness, .0f, 6.f);
    ImGui::SetItemTooltip("In pixels of a picture 1080 high. Zero draws no lines.");
    field::SliderFloat("Outline Threshold", &edited.inkDepthThreshold, .005f, .5f);
    field::SliderFloat("Crease Threshold", &edited.inkCreaseThreshold, .0f, 4.f);
    ImGui::SetItemTooltip("How sharply a surface must fold to be inked. A right angle seen corner on\nmeasures about one.");
    field::ColorEdit3("Ink", &edited.inkColor.x);
    field::SliderFloat("Ink Tint", &edited.inkTint, .0f, 1.f);
    ImGui::SetItemTooltip("How much of the colour beneath it the ink takes on.");
    field::DragFloat("Fade Distance", &edited.inkFadeDistance, .5f, 1.f, 1000.f, "%.0f");
  }
  if (field::Fold("Grade", true)) {
    field::SliderFloat("Saturation", &edited.saturation, .0f, 2.f);
    field::SliderFloat("Contrast", &edited.contrast, .5f, 2.f);
    field::ColorEdit3("Shadows", &edited.shadowColor.x);
    field::ColorEdit3("Highlights", &edited.highlightColor.x);
    ImGui::SetItemTooltip("Grey is no push. The darks and the lights are each pushed towards their colour\nby as much as they are dark or light.");
    field::SliderFloat("Vignette", &edited.vignette, .0f, 1.f);
  }
  if (edited != *style)
    *style = edited;
}
template <>
inline auto PropertyDisplayer::operator()<kuki::PlanarReflection>(kuki::PlanarReflection *surface) -> void {
  if (!surface || !context.capabilities.stylization)
    return;
  auto edited = *surface;
  if (field::Fold("Surface", true)) {
    field::DragFloat("Height", &edited.height, .01f, -100.f, 100.f, "%.2f");
    field::DragFloat("Tolerance", &edited.tolerance, .001f, .0001f, 1.f, "%.3f");
    ImGui::SetItemTooltip("How far from the height a pixel may be and still be taken for the surface.");
    field::SliderFloat("Strength", &edited.strength, .0f, 1.f);
    ImGui::SetItemTooltip("Zero switches the reflection off.");
    field::SliderFloat("Dry Reflectance", &edited.dryReflectance, .0f, 1.f);
    field::SliderFloat("Blur", &edited.blur, .0f, 1.f);
    field::DragFloat("Max Distance", &edited.maxDistance, .5f, 1.f, 500.f, "%.0f");
  }
  if (field::Fold("Puddles", true)) {
    field::SliderFloat("Coverage", &edited.puddleCoverage, .0f, 1.f);
    field::SliderFloat("Scale", &edited.puddleScale, .01f, 4.f);
    field::SliderFloat("Ripples", &edited.ripples, .0f, 4.f);
    ImGui::SetItemTooltip("How hard rain disturbs the puddles. Zero is still water.");
    field::SliderFloat("Ripple Density", &edited.rippleDensity, .1f, 10.f);
  }
  if (edited != *surface)
    *surface = edited;
}
template <>
inline auto PropertyDisplayer::operator()<kuki::VolumetricFog>(kuki::VolumetricFog *fog) -> void {
  if (!fog)
    return;
  if (!context.capabilities.volumetrics)
    return;
  auto edited = *fog;
  if (field::Fold("Medium", true)) {
    field::DragFloat("Density", &edited.density, .002f, .0f, 2.f, "%.3f");
    ImGui::SetItemTooltip("Extinction at the base height, per scene unit. Zero switches the scene's own\nmedium off entirely and leaves only the aerial perspective below, which is what\nan outdoor scene usually wants -- two media over the same air is haze counted\ntwice.");
    field::DragFloat("Base Height", &edited.baseHeight, .05f, -100.f, 100.f, "%.2f");
    field::SliderFloat("Height Falloff", &edited.heightFalloff, .0f, 4.f);
    ImGui::SetItemTooltip("How quickly the air thins going up. This is the reason a shaft is brightest near\nthe floor: an even medium gives a cone of uniform brightness, which reads as a\nsolid object rather than as light.");
    field::ColorEdit3("Albedo", &edited.albedo.x);
    ImGui::SetItemTooltip("What fraction of what the medium removes it scatters back, per channel -- so it\ncannot brighten what is behind it. Slightly blue keeps a white shaft from\nreading as smoke.");
    field::SliderFloat("Anisotropy", &edited.anisotropy, -.9f, .9f);
    ImGui::SetItemTooltip("Modest on purpose. High values make a shaft visible only when looking nearly\ninto its light, which is correct for genuine haze and disappointing in a room,\nwhere the beam is meant to be seen from the side.");
  }
  if (field::Fold("Light", true)) {
    field::ColorEdit3("Ambient", &edited.ambient.x);
    ImGui::SetItemTooltip("Light arriving from everywhere rather than from a light. Without it the fog is\nblack wherever no light reaches it, so a room with one lamp gets a bright cone\nstanding in a void.");
    field::SliderFloat("Light Scale", &edited.lightScale, .0f, 4.f);
    ImGui::SetItemTooltip("A viewing decision rather than a fact about the air: the physically right shaft\nis often a faint one, and a scene that wants the beam to be the subject needs to\nsay so without lying about the density.");
  }
  if (field::Fold("Distance", true)) {
    field::DragFloat("Max Distance", &edited.maxDistance, .5f, 1.f, 5000.f, "%.0f");
    ImGui::SetItemTooltip("How far the march goes. Beyond it the medium is taken to continue unchanged, so\na distant surface still fogs -- what is lost past here is the shadowing, not the\nhaze. The step count is fixed, so doubling this halves every shaft's resolution.");
    field::SliderFloat("Aerial Perspective", &edited.aerialPerspective, .0f, 2.f);
    ImGui::SetItemTooltip("The atmosphere's own haze, applied over the distance to each surface using the\nsame figures the sky was marched from -- which is what turns a far wall blue and\nis most of why distance reads as distance outdoors.\n\nDoes nothing without an Atmosphere Sky.");
    field::DragFloat("Kilometres / Unit", &edited.kilometresPerUnit, .0005f, .0f, 1.f, "%.4f");
    ImGui::SetItemTooltip("The one place the scene's units and the atmosphere's have to agree, because how\nblue a far wall goes depends on how far away the wall really is. Raise it for a\nscene meant to read as a landscape.\n\nRead only by Aerial Perspective above.");
  }
  if (field::Fold("Quality")) {
    auto steps = static_cast<int>(edited.steps);
    if (field::SliderInt("Steps", &steps, 4, 128))
      edited.steps = static_cast<uint32_t>(std::max(4, steps));
    ImGui::SetItemTooltip("The march is dithered per pixel, so too few steps costs a fine grain rather than\nvisible bands.");
  }
  if (edited != *fog)
    *fog = edited;
}
template <>
inline auto PropertyDisplayer::operator()<kuki::ParticleEmitter>(kuki::ParticleEmitter *emitter) -> void {
  if (!emitter)
    return;
  if (!context.capabilities.particles)
    return;
  auto edited = *emitter;
  // Written over the whole component, since that is what a preset is -- except the two fields that
  // are not a description of an effect: which mesh, and how many bursts have gone off.
  if (field::BeginCombo("Preset", "Choose...")) {
    for (const auto &preset : kuki::PARTICLE_PRESETS)
      if (ImGui::Selectable(preset.name)) {
        const auto mesh = edited.mesh;
        const auto fired = edited.fired;
        edited = preset.make();
        edited.mesh = mesh;
        edited.fired = fired;
      }
    ImGui::EndCombo();
  }
  ImGui::SetItemTooltip("Replaces every setting below with a starting point for one kind of effect.");
  if (field::Fold("Emission", true)) {
    static auto &shapes = kuki::EnumTraits<kuki::ParticleShape>::GetNames();
    auto shape = static_cast<int>(edited.shape);
    if (field::Combo("Shape", &shape, shapes.data(), static_cast<int>(shapes.size())))
      edited.shape = static_cast<kuki::ParticleShape>(shape);
    ImGui::SetItemTooltip("Where particles are born: inside a sphere around the emitter, or on the\nsurface of a mesh, thrown along its normals.");
    if (edited.shape == kuki::ParticleShape::Mesh) {
      auto mesh = edited.mesh;
      field::InputScalar("Mesh", ImGuiDataType_U64, &mesh, nullptr, nullptr, nullptr, ImGuiInputTextFlags_ReadOnly);
      if (ImGui::BeginDragDropTarget()) {
        if (const auto *payload = ImGui::AcceptDragDropPayload("##SpawnPayload"); payload && payload->DataSize == sizeof(kuki::AssetID))
          edited.mesh = *static_cast<const kuki::AssetID *>(payload->Data);
        ImGui::EndDragDropTarget();
      }
      ImGui::SetItemTooltip("Drop a mesh here from the asset browser. It is read and never altered, and it\nis not drawn: add a mesh to the entity as well to see it.\n\nAnything that is not a mesh leaves the emitter on the sphere.");
    }
    field::Toggle(ICON_LC_SPARKLES, "Emitting", &edited.emitting);
    ImGui::SetItemTooltip("Stops new particles. The ones in flight live out their time.");
    field::DragFloat("Rate", &edited.rate, 5.f, .0f, 1.0e6f, "%.0f /s");
    auto burst = static_cast<int>(edited.burst);
    if (field::DragInt("Burst", &burst, 16.f, 0, 1 << 20))
      edited.burst = static_cast<uint32_t>(std::clamp(burst, 0, 1 << 20));
    ImGui::SetItemTooltip("Particles spawned at once each time the emitter is fired, whatever the rate\nis doing. No more than the capacity.");
    ImGui::SameLine();
    if (ImGui::Button("Fire"))
      ++edited.fired;
    auto capacity = static_cast<int>(edited.capacity);
    if (field::DragInt("Capacity", &capacity, 64.f, 1, 1 << 20))
      edited.capacity = static_cast<uint32_t>(std::clamp(capacity, 1, 1 << 20));
    ImGui::SetItemTooltip("Slots in the pool. Every one is simulated and submitted each frame whether or\nnot anything is alive in it, so this is what the emitter costs.\n\nChanging it discards the particles in flight.");
    if (edited.shape == kuki::ParticleShape::Sphere) {
      field::DragFloat("Radius", &edited.radius, .005f, .0f, 100.f, "%.3f");
      ImGui::SetItemTooltip("Particles appear somewhere inside a sphere this size around the emitter.");
    }
    if (edited.shape == kuki::ParticleShape::Box) {
      field::DragFloat3("Extent", &edited.extent.x, .05f, .0f, 1000.f, "%.2f");
      ImGui::SetItemTooltip("Half the size of the box particles appear in, along the emitter's own axes.\nA box with no height covers a floor.");
    }
  }
  if (field::Fold("Motion", true)) {
    field::DragFloat(ICON_LC_TIMER " Lifetime", &edited.lifetime, .01f, .01f, 600.f, "%.2f s");
    field::SliderFloat("Lifetime Variance", &edited.lifetimeVariance, .0f, 1.f);
    ImGui::SetItemTooltip("How much of its lifetime a particle may fall short by. Only ever shortens, so\nthe figure above stays the longest any of them lasts.");
    field::DragFloat("Hold", &edited.hold, .01f, .0f, 600.f, "%.2f s");
    ImGui::SetItemTooltip("How long a particle waits where it was born before it starts to move. Held, it\nis drawn at its starting size and colour and does not age.");
    field::SliderFloat("Hold Variance", &edited.holdVariance, .0f, 1.f);
    ImGui::SetItemTooltip("How much of the hold a particle may fall short by, which is what lets a burst\ngo a piece at a time.");
    field::DragFloat(ICON_LC_GAUGE " Speed", &edited.speed, .02f, .0f, 1000.f, "%.2f");
    field::SliderFloat("Speed Variance", &edited.speedVariance, .0f, 1.f);
    field::SliderFloat("Spread", &edited.spread, .0f, 180.f, "%.0f deg");
    ImGui::SetItemTooltip("Half-angle of the cone particles are thrown into, around the local +Y or the\nmesh's normal. Nought is a jet, ninety a hemisphere and a hundred and eighty\nevery direction at once.");
    field::DragFloat3(ICON_LC_ARROW_DOWN " Gravity", &edited.gravity.x, .05f, -100.f, 100.f, "%.2f");
    ImGui::SetItemTooltip("Any constant acceleration. Point it upwards and a little weaker for embers\ncarried by their own heat.");
    field::SliderFloat("Drag", &edited.drag, .0f, 10.f);
    ImGui::SetItemTooltip("How quickly the air takes a particle's speed away. Without it every particle\nfollows a clean parabola, and a spray of clean parabolas is water.");
    field::SliderFloat("Turbulence", &edited.turbulence, .0f, 20.f);
    ImGui::SetItemTooltip("How hard a gust pushes each particle about, each to a rhythm of its own. What\nmakes snow drift and petals wander.");
  }
  if (field::Fold("Appearance", true)) {
    static auto &blends = kuki::EnumTraits<kuki::ParticleBlend>::GetNames();
    auto blend = static_cast<int>(edited.blend);
    if (field::Combo("Blend", &blend, blends.data(), static_cast<int>(blends.size())))
      edited.blend = static_cast<kuki::ParticleBlend>(blend);
    ImGui::SetItemTooltip("Additive particles brighten what is behind them and can do nothing else.\nOpaque ones are solid discs, which is the one way to draw a dark particle;\nborn on a mesh they are shaded once by the sun and keep that shade. Alpha ones\nare a faint veil, for mist and steam.");
    static auto &facings = kuki::EnumTraits<kuki::ParticleFacing>::GetNames();
    auto facing = static_cast<int>(edited.facing);
    if (field::Combo("Facing", &facing, facings.data(), static_cast<int>(facings.size())))
      edited.facing = static_cast<kuki::ParticleFacing>(facing);
    static auto &sprites = kuki::EnumTraits<kuki::ParticleSprite>::GetNames();
    auto sprite = static_cast<int>(edited.sprite);
    if (field::Combo("Sprite", &sprite, sprites.data(), static_cast<int>(sprites.size())))
      edited.sprite = static_cast<kuki::ParticleSprite>(sprite);
    if (edited.facing == kuki::ParticleFacing::Velocity) {
      field::DragFloat("Stretch", &edited.stretch, .001f, .0f, 1.f, "%.3f s");
      ImGui::SetItemTooltip("How many seconds of its own travel a streak is long. A raindrop is about a\nsixtieth: what a shutter would smear it over.");
    }
    if (edited.blend == kuki::ParticleBlend::Alpha)
      field::SliderFloat("Opacity", &edited.opacity, .0f, 1.f);
    field::DragFloat("Start Size", &edited.startSize, .002f, .0f, 100.f, "%.3f");
    field::DragFloat("End Size", &edited.endSize, .002f, .0f, 100.f, "%.3f");
    field::ColorEdit3(ICON_LC_PALETTE " Start Colour", &edited.startColor.x);
    field::ColorEdit3(ICON_LC_PALETTE " End Colour", &edited.endColor.x);
    field::DragFloat(ICON_LC_SUN " Intensity", &edited.intensity, .05f, .0f, 1000.f, "%.2f");
    ImGui::SetItemTooltip("What the colours are multiplied by. Drawn ahead of the bloom chain, so an\nadditive particle only glows if this takes it past white -- which is a statement\nabout how much light there is rather than what colour it is.");
  }
  if (edited != *emitter)
    *emitter = edited;
}
template <>
inline auto PropertyDisplayer::operator()<kuki::IndirectLighting>(kuki::IndirectLighting *indirect) -> void {
  if (!indirect)
    return;
  // Grouped by what moving one costs, which is the only thing that separates them and the thing you
  // need to know before you start dragging. Each group says its cost once, at the top, rather than
  // every slider repeating it.
  //
  // Edited through a copy and written back, as every other displayer here does: a widget writes on
  // the frame it is dragged, and handing it the live field would let it write while the renderer is
  // reading.
  //
  // Most of them describe a probe field, so most of them are absent on a backend without one. What
  // survives is `Sky` and `Ambient Fallback`: light that arrives by a route the OpenGL backend also
  // has, and which it now reads -- see `GLLitShader::SetIndirectLighting`. Hidden rather than
  // disabled, because a greyed row invites you to work out how to un-grey it and there is no answer
  // short of restarting on the other backend.
  const auto probes = context.capabilities.probeVolume;
  const auto defaults = kuki::IndirectLighting{};
  auto edited = *indirect;
  if (field::Fold("Terms", true)) {
    if (probes) {
      field::SliderFloat("Bounce", &edited.bounceIntensity, .0f, 4.f);
      ImGui::SetItemTooltip("How much of the probe volume's bounce reaches the picture.\nThe first thing to reach for: most of the time the question is\nwhether there is too much indirect light or too little.");
    }
    field::SliderFloat("Sky", &edited.skyIntensity, .0f, 4.f);
    ImGui::SetItemTooltip("The same, for the sky's diffuse contribution. Separate from the\nbounce because the two arrive by different routes, and finding the\nbalance between them needs both.");
    field::SliderFloat("Ambient Fallback", &edited.ambientFallback, .0f, .5f, "%.3f");
    ImGui::SetItemTooltip("The flat ambient a scene falls back on with no sky, no ambient\nlight and no probe volume. Shades nothing when any of the three\nis present.");
    field::ColorEdit3(ICON_LC_PALETTE " Background", &edited.backgroundColor.x, ImGuiColorEditFlags_Float);
    ImGui::SetItemTooltip("What fills the frame behind everything with no skybox. Linear,\nand tone mapped along with the rest of the picture -- so the swatch\nis not what reaches the display. The other half of Ambient Fallback:\nthis is what a scene stands in front of, and that is the light it\ngets for standing there.");
  }
  if (probes && field::Fold("Reconstruction", true)) {
    field::SliderFloat("Surface Bias", &edited.probeSurfaceBias, .0f, 1.f);
    ImGui::SetItemTooltip("A step off the surface before the field is sampled, as a fraction\nof the local cell. Too small and a concave corner reads as occluded\nfrom every probe around it; too large and the bounce detaches from\nthe geometry, losing the darkening where surfaces meet.");
    field::SliderFloat("Weight Floor", &edited.probeWeightFloor, .0f, 1.f);
    ImGui::SetItemTooltip("Below this a probe the point can barely see is crushed towards\nnothing rather than merely reduced. Raise it to stop light coming\nthrough a thin wall; watch for the probe lattice printing itself\nonto flat surfaces as single corners start deciding whole cells.");
    field::SliderFloat("Visibility Sharpness", &edited.probeVisibilitySharpness, 1.f, 8.f);
    ImGui::SetItemTooltip("How sharply a partly-visible probe is disbelieved. One is the\nstatistical answer and it leaks; higher distrusts a partial verdict\nmore, at the cost of darkening the creases the test is least\ncertain about.");
  }
  if (probes && field::Fold("Trace")) {
    field::SliderFloat("Depth Sharpness", &edited.probeDepthSharpness, 4.f, 64.f);
    ImGui::SetItemTooltip("How tightly a ray must line up with a visibility texel to count\ntowards it. Tied to the map's width rather than free: 28 puts the\nlobe's half-width at the angle one texel subtends, and sharpening\nmuch past it leaves texels that no ray feeds.");
    field::SliderFloat("Depth Range", &edited.probeDepthRange, .05f, 2.f);
    ImGui::SetItemTooltip("How far a probe's visibility reaches, as a fraction of the volume's\nside. Sited too close and probes invent an occluder in a ring at\nwhatever radius the clamp begins to bite.");
    field::SliderFloat("Ray Distance", &edited.rayDistanceScale, .25f, 4.f);
    ImGui::SetItemTooltip("How far a ray may travel, as a multiple of the volume's side.\nThe cheapest thing here to shorten: a ray that reaches nothing\ncosts the same as one that does.");
    field::SliderFloat("Hit Normal Nudge", &edited.hitNormalNudge, .0f, 1.f);
    ImGui::SetItemTooltip("How far a hit is nudged along its normal before it is shaded, as a\nfraction of a cell. The trace's equivalent of a shadow bias.");
    field::SliderFloat("Opaque Threshold", &edited.opaqueThreshold, .001f, .5f, "%.3f");
    ImGui::SetItemTooltip("How much light must still be getting through before a ray stops\nbeing followed. Purely a cost control: raising it ends alpha-blended\nchains sooner.");
    auto meanStep = static_cast<int>(edited.meanStep);
    if (field::SliderInt("Mean Step", &meanStep, 1, 16))
      edited.meanStep = static_cast<uint32_t>(std::max(meanStep, 1));
    ImGui::SetItemTooltip("Estimates the running mean is worth, so a fresh one is folded in at\none over n plus this. Lower converges faster and holds more of the\nray set's noise; higher is steadier and slower to notice that the\nlight changed.");
  }
  if (probes && field::Fold("Probe Placement")) {
    field::SliderFloat("Relocation Margin", &edited.relocationMargin, .0f, 1.f);
    ImGui::SetItemTooltip("How far a probe steps towards open space each trace, as a fraction\nof its allowance.");
    field::SliderFloat("Relocation Damping", &edited.relocationDamping, .0f, 1.f);
    ImGui::SetItemTooltip("How much of each step is held back, which is what keeps a probe\nfrom oscillating between two placements.");
    field::SliderFloat("Escape Agreement", &edited.escapeAgreement, .01f, 1.f);
    ImGui::SetItemTooltip("How much the rays coming back off a far side must agree before\ntheir mean is taken for a direction rather than the residue of a\nray set. A probe just outside a surface scores about a half; one\nsealed inside solid geometry, about 0.0016.");
    field::SliderFloat("Buried Limit", &edited.buriedLimit, .01f, 1.f);
    ImGui::SetItemTooltip("What share of a probe's rays may come back off the far side of a\nsurface before it is disbelieved and filled in from its neighbours.\nThe Probe View's Trust and Classification entries show the verdict.");
  }
  ImGui::Separator();
  ImGui::BeginDisabled(std::memcmp(&edited, &defaults, sizeof(edited)) == 0);
  if (ImGui::Button("Reset To Defaults"))
    edited = defaults;
  ImGui::EndDisabled();
  ImGui::SetItemTooltip("The constants every one of these was before it was a setting.");
  *indirect = edited;
}
template <>
inline auto PropertyDisplayer::operator()<kuki::Light>(kuki::Light *light) -> void {
  static constexpr auto MAX_FLOAT = std::numeric_limits<float>::max();
  if (!light)
    return;
  static auto &types = kuki::EnumTraits<kuki::LightType>::GetNames();
  auto type = static_cast<int>(light->type);
  if (field::Combo("Type", &type, types.data(), types.size()))
    light->type = static_cast<kuki::LightType>(type);
  auto position = light->position;
  if (field::DragFloat3(ICON_LC_MOVE_3D " Position", glm::value_ptr(position), .1f))
    light->position = position;
  auto rotationQuat = light->rotation;
  auto rotationDegrees = glm::degrees(glm::eulerAngles(rotationQuat));
  if (field::DragFloat3(ICON_LC_ROTATE_3D " Rotation", glm::value_ptr(rotationDegrees), .1f)) {
    for (auto i = 0; i < 3; ++i) {
      auto angle = rotationDegrees[i];
      while (angle > 180.f)
        angle -= 360.f;
      while (angle < -180.f)
        angle += 360.f;
    }
    auto rotationRadians = glm::radians(rotationDegrees);
    light->SetRotation(glm::quat(rotationRadians));
  }
  auto ambient = light->ambient;
  if (field::ColorEdit3(ICON_LC_PALETTE " Ambient Color", glm::value_ptr(ambient)))
    light->ambient = ambient;
  auto diffuse = light->diffuse;
  if (field::ColorEdit3(ICON_LC_PALETTE " Diffuse Color", glm::value_ptr(diffuse)))
    light->diffuse = diffuse;
  auto specular = light->specular;
  if (field::ColorEdit3(ICON_LC_PALETTE " Specular Color", glm::value_ptr(specular)))
    light->specular = specular;
  auto intensity = light->intensity;
  if (field::DragFloat(ICON_LC_SUN " Intensity", &intensity, .05f, .0f, MAX_FLOAT, "%.2f"))
    light->intensity = intensity;
  if (light->type == kuki::LightType::Directional) {
    auto angularDiameter = light->angularDiameter;
    if (field::DragFloat("Angular Diameter", &angularDiameter, .01f, .0f, 30.f, "%.2f deg"))
      light->angularDiameter = std::clamp(angularDiameter, .0f, 30.f);
    ImGui::SetItemTooltip("How wide the light is in the sky, which is what makes its shadows soften the\nfurther they fall. The sun is a little over half a degree.\n\nNought is a light with no size: every shadow the same fixed width.");
  }
  if (light->type == kuki::LightType::Spot) {
    auto sourceRadius = light->sourceRadius;
    if (field::DragFloat("Source Radius", &sourceRadius, .005f, .0f, MAX_FLOAT, "%.3f"))
      light->sourceRadius = std::max(sourceRadius, .0f);
    ImGui::SetItemTooltip("The size of the bulb, in scene units. A larger one gives shadows that are\nsharp at the foot of what casts them and soft further away.\n\nNought is a point: every shadow the same fixed width.");
  }
  if (light->type == kuki::LightType::Point || light->type == kuki::LightType::Spot) {
    constexpr auto MIN_RANGE = 1.f;
    const auto currentLinear = std::max(light->linear, 1.0e-4f);
    const auto currentRange = 4.5f / currentLinear;
    auto range = currentRange;
    if (field::DragFloat("Range", &range, .5f, MIN_RANGE, MAX_FLOAT, "%.1f")) {
      range = std::max(range, MIN_RANGE);
      light->constant = 1.f;
      light->linear = 4.5f / range;
      light->quadratic = 75.f / (range * range);
    }
    // The three terms Range drives, shown alongside it rather than behind a fold. Editing one by
    // hand puts the falloff somewhere Range cannot express, and the next drag of Range overwrites
    // all three; that is the intended relationship, and it is easier to see when both are visible.
    auto constant = light->constant;
    if (field::SliderFloat("Constant Term", &constant, .0f, 1.f))
      light->constant = constant;
    auto linear = light->linear;
    if (field::SliderFloat("Linear Term", &linear, .0f, 1.f))
      light->linear = linear;
    auto quadratic = light->quadratic;
    if (field::SliderFloat("Quadratic Term", &quadratic, .0f, 1.f))
      light->quadratic = quadratic;
  }
  if (light->type == kuki::LightType::Spot) {
    auto innerAngle = glm::degrees(std::acos(std::clamp(light->innerCutoff, -1.f, 1.f)));
    auto outerAngle = glm::degrees(std::acos(std::clamp(light->outerCutoff, -1.f, 1.f)));
    if (field::DragFloat("Inner Cone Angle", &innerAngle, .1f, .0f, outerAngle))
      light->innerCutoff = std::cos(glm::radians(innerAngle));
    if (field::DragFloat("Outer Cone Angle", &outerAngle, .1f, innerAngle, 89.f))
      light->outerCutoff = std::cos(glm::radians(outerAngle));
    auto spotNearPlane = light->nearPlane;
    if (field::DragFloat("Shadow Near Plane", &spotNearPlane, .1f, .0f, MAX_FLOAT))
      light->nearPlane = spotNearPlane;
    auto spotFarPlane = light->farPlane;
    if (field::DragFloat("Shadow Far Plane", &spotFarPlane, .1f, .0f, MAX_FLOAT))
      light->farPlane = spotFarPlane;
  }
  if (light->type == kuki::LightType::Directional) {
    auto nearPlane = light->nearPlane;
    if (field::DragFloat("Shadow Near Plane", &nearPlane, .1f, .0f, MAX_FLOAT))
      light->nearPlane = nearPlane;
    auto farPlane = light->farPlane;
    if (field::DragFloat("Shadow Far Plane", &farPlane, .1f, .0f, MAX_FLOAT))
      light->farPlane = farPlane;
    auto orthoSize = light->orthoSize;
    if (field::DragFloat("Shadow Coverage Size", &orthoSize, .1f, .0f, MAX_FLOAT))
      light->orthoSize = orthoSize;
  }
}
template <>
inline auto PropertyDisplayer::operator()<kuki::MaterialHandle>(kuki::MaterialHandle *handle) -> void {
  if (!handle)
    return;
  auto assetId = handle->assetId;
  field::InputScalar("Asset ID", ImGuiDataType_U64, &assetId, nullptr, nullptr, nullptr, ImGuiInputTextFlags_ReadOnly);
}
template <>
inline auto PropertyDisplayer::operator()<kuki::MeshHandle>(kuki::MeshHandle *handle) -> void {
  if (!handle)
    return;
  auto assetId = handle->assetId;
  field::InputScalar("Asset ID", ImGuiDataType_U64, &assetId, nullptr, nullptr, nullptr, ImGuiInputTextFlags_ReadOnly);
}
template <>
inline auto PropertyDisplayer::operator()<kuki::ModelMaterialHandle>(kuki::ModelMaterialHandle *handle) -> void {
  if (!handle)
    return;
  auto modelAssetId = handle->modelAssetId;
  field::InputScalar("Model Asset ID", ImGuiDataType_U64, &modelAssetId, nullptr, nullptr, nullptr, ImGuiInputTextFlags_ReadOnly);
  auto materialIndex = handle->materialIndex;
  field::InputScalar("Material Index", ImGuiDataType_U64, &materialIndex, nullptr, nullptr, nullptr, ImGuiInputTextFlags_ReadOnly);
}
template <>
inline auto PropertyDisplayer::operator()<kuki::ModelMeshHandle>(kuki::ModelMeshHandle *handle) -> void {
  if (!handle)
    return;
  auto modelAssetId = handle->modelAssetId;
  field::InputScalar("Model Asset ID", ImGuiDataType_U64, &modelAssetId, nullptr, nullptr, nullptr, ImGuiInputTextFlags_ReadOnly);
  auto meshIndex = handle->meshIndex;
  field::InputScalar("Mesh Index", ImGuiDataType_U64, &meshIndex, nullptr, nullptr, nullptr, ImGuiInputTextFlags_ReadOnly);
}
/// @brief What the running backend resolved the entity's handles to: its own material and mesh.
///
/// They were components of their own, one per backend, and had panels of their own. They are one
/// component now, of which only the running backend's half is ever filled in, so that half is what
/// is shown -- with the same panels.
template <>
inline auto PropertyDisplayer::operator()<kuki::RenderBinding>(kuki::RenderBinding *binding) -> void {
  if (!binding)
    return;
  if (binding->glMaterialBound || binding->glMesh) {
    if (binding->glMaterialBound && field::Fold("Material", true))
      (*this)(&binding->glMaterial);
    if (binding->glMesh && field::Fold("Mesh", false))
      (*this)(&binding->glMesh);
  } else if (binding->dxMaterialBound)
    (*this)(&binding->dxMaterial);
  else
    ImGui::TextDisabled("Not resolved by the running backend yet.");
}
template <>
inline auto PropertyDisplayer::operator()<kuki::Skeleton>(kuki::Skeleton *skeleton) -> void {
  if (!skeleton)
    return;
  auto nodeCount = static_cast<int>(skeleton->nodeEntities.size());
  field::InputInt("Node Count", &nodeCount, 0, 0, ImGuiInputTextFlags_ReadOnly);
}
template <>
inline auto PropertyDisplayer::operator()<kuki::SkyboxHandle>(kuki::SkyboxHandle *handle) -> void {
  if (!handle)
    return;
  auto assetId = handle->assetId;
  field::InputScalar("Asset ID", ImGuiDataType_U64, &assetId, nullptr, nullptr, nullptr, ImGuiInputTextFlags_ReadOnly);
}
template <>
inline auto PropertyDisplayer::operator()<kuki::TextureHandle>(kuki::TextureHandle *handle) -> void {
  if (!handle)
    return;
  auto assetId = handle->assetId;
  field::InputScalar("Asset ID", ImGuiDataType_U64, &assetId, nullptr, nullptr, nullptr, ImGuiInputTextFlags_ReadOnly);
}
template <>
inline auto PropertyDisplayer::operator()<kuki::Transform>(kuki::Transform *transform) -> void {
  static constexpr auto MAX_FLOAT = std::numeric_limits<float>::max();
  if (!transform)
    return;
  auto dirty = false;
  auto position = transform->position;
  if (field::DragFloat3(ICON_LC_MOVE_3D " Position", glm::value_ptr(position), .1f)) {
    transform->position = position;
    dirty = true;
  }
  auto rotationQuat = transform->rotation;
  auto rotationDegrees = glm::degrees(glm::eulerAngles(rotationQuat));
  if (field::DragFloat3(ICON_LC_ROTATE_3D " Rotation", glm::value_ptr(rotationDegrees), .1f)) {
    for (auto i = 0; i < 3; ++i) {
      auto angle = rotationDegrees[i];
      while (angle > 180.f)
        angle -= 360.f;
      while (angle < -180.f)
        angle += 360.f;
    }
    auto rotationRadians = glm::radians(rotationDegrees);
    transform->rotation = glm::quat(rotationRadians);
    dirty = true;
  }
  auto scale = transform->scale;
  static auto uniformMode = true;
  if (uniformMode) {
    auto uniformScale = scale.x;
    if (field::DragFloat(ICON_LC_SCALE_3D " Scale", &uniformScale, .1f, .0f, MAX_FLOAT)) {
      transform->scale = glm::vec3(uniformScale);
      dirty = true;
    }
    ImGui::SameLine();
    field::Toggle(ICON_LC_LINK, "Uniform", &uniformMode);
  } else {
    if (field::DragFloat3(ICON_LC_SCALE_3D " Scale", glm::value_ptr(scale), .1f, .0f, MAX_FLOAT)) {
      transform->scale = scale;
      dirty = true;
    }
    ImGui::SameLine();
    field::Toggle(ICON_LC_LINK, "Uniform", &uniformMode);
  }
  if (dirty)
    app.MarkTransformDirty(context.selectedEntityId);
}
