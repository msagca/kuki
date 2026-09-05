#pragma once
#include <algorithm>
#include <animator.hpp>
#include <application.hpp>
#include <array>
#include <bone_data.hpp>
#include <bounding_box.hpp>
#include <camera.hpp>
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
#include <skeleton.hpp>
#include <skybox_handle.hpp>
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
inline auto DisplayMaterialSurface(kuki::MaterialType &type, kuki::MaterialFallback &fallback) -> void {
  static auto &types = kuki::EnumTraits<kuki::MaterialType>::GetNames();
  auto typeIndex = static_cast<int>(type);
  if (ImGui::Combo("Type", &typeIndex, types.data(), types.size()))
    type = static_cast<kuki::MaterialType>(typeIndex);
  auto albedoColor = fallback.albedo;
  if (ImGui::ColorEdit4("Albedo Color", glm::value_ptr(albedoColor)))
    fallback.albedo = albedoColor;
  auto specularColor = fallback.specular;
  if (ImGui::ColorEdit4("Specular Color", glm::value_ptr(specularColor)))
    fallback.specular = specularColor;
  auto emissiveColor = fallback.emissive;
  if (ImGui::ColorEdit4("Emissive Color", glm::value_ptr(emissiveColor)))
    fallback.emissive = emissiveColor;
  auto metalnessFactor = fallback.metalness;
  if (ImGui::SliderFloat("Metalness Factor", &metalnessFactor, .0f, 1.f))
    fallback.metalness = metalnessFactor;
  auto occlusionFactor = fallback.occlusion;
  if (ImGui::SliderFloat("Occlusion Factor", &occlusionFactor, .0f, 1.f))
    fallback.occlusion = occlusionFactor;
  auto roughnessFactor = fallback.roughness;
  if (ImGui::SliderFloat("Roughness Factor", &roughnessFactor, .0f, 1.f))
    fallback.roughness = roughnessFactor;
  static auto &alphaModes = kuki::EnumTraits<kuki::AlphaMode>::GetNames();
  auto alphaMode = static_cast<int>(fallback.alphaMode);
  if (ImGui::Combo("Alpha Mode", &alphaMode, alphaModes.data(), alphaModes.size()))
    fallback.alphaMode = static_cast<kuki::AlphaMode>(alphaMode);
  if (fallback.alphaMode == kuki::AlphaMode::Mask) {
    auto alphaCutoff = fallback.alphaCutoff;
    if (ImGui::SliderFloat("Alpha Cutoff", &alphaCutoff, .0f, 1.f))
      fallback.alphaCutoff = alphaCutoff;
  }
  auto ior = fallback.ior;
  if (ImGui::SliderFloat("Index of Refraction", &ior, 1.f, 3.f))
    fallback.ior = ior;
  ImGui::SetItemTooltip("Also fixes how reflective the surface is head on, so it matters to every\nmaterial rather than only the transmissive ones. 1.5 is the usual 0.04.");
  auto transmission = fallback.transmission;
  if (ImGui::SliderFloat("Transmission", &transmission, .0f, 1.f))
    fallback.transmission = transmission;
  if (fallback.transmission > .0f) {
    auto thickness = fallback.thickness;
    if (ImGui::SliderFloat("Thickness", &thickness, .0f, 10.f))
      fallback.thickness = thickness;
    auto attenuationColor = glm::vec3(fallback.attenuation);
    if (ImGui::ColorEdit3("Attenuation Color", glm::value_ptr(attenuationColor)))
      fallback.attenuation = glm::vec4(attenuationColor, fallback.attenuation.w);
    auto attenuationDistance = fallback.attenuation.w;
    if (ImGui::SliderFloat("Attenuation Distance", &attenuationDistance, .0f, 10.f))
      fallback.attenuation.w = attenuationDistance;
  }
}
template <typename T>
inline auto PropertyDisplayer::operator()(T *) -> void {}
template <>
inline auto PropertyDisplayer::operator()<kuki::Animator>(kuki::Animator *animator) -> void {
  if (!animator)
    return;
  const auto modelName = app.GetAssetName(animator->modelAssetId);
  ImGui::LabelText("Model", "%s", modelName.empty() ? "(none)" : modelName.c_str());
  // The clip list comes from the model, so everything below it depends on that model being loaded.
  // Without it there is nothing to name the clips with and nothing to bound the scrub by, so the
  // raw index is offered on its own rather than a set of controls that cannot describe themselves.
  const auto *model = app.GetAsset<kuki::ModelAsset>(animator->modelAssetId);
  if (!model || model->animations.empty()) {
    auto clipIndex = animator->clipIndex;
    if (ImGui::InputInt("Clip", &clipIndex))
      animator->clipIndex = clipIndex;
    ImGui::TextDisabled(model ? "This model has no animation clips." : "Model asset is not loaded.");
    return;
  }
  const auto clipCount = static_cast<int>(model->animations.size());
  const auto valid = animator->clipIndex >= 0 && animator->clipIndex < clipCount;
  const auto *preview = valid ? model->animations[animator->clipIndex].name.c_str() : "(none)";
  if (ImGui::BeginCombo("Clip", preview)) {
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
  if (ImGui::Checkbox("Playing", &playing))
    animator->playing = playing;
  ImGui::SameLine();
  auto loop = animator->loop;
  if (ImGui::Checkbox("Loop", &loop))
    animator->loop = loop;
  // Scrubbed in ticks because that is what the animation system advances and what the clip's
  // duration is measured in; the seconds underneath are the part a person can judge, and are only
  // meaningful while the clip declares a rate to convert by.
  auto time = animator->time;
  if (ImGui::SliderFloat("Time", &time, .0f, clip.duration, "%.1f ticks", ImGuiSliderFlags_AlwaysClamp))
    animator->time = time;
  if (clip.ticksPerSecond > .0f)
    ImGui::TextDisabled("%.2f s of %.2f s, at %.0f ticks per second", animator->time / clip.ticksPerSecond, clip.duration / clip.ticksPerSecond, clip.ticksPerSecond);
  ImGui::TextDisabled("%zu channels", clip.channels.size());
}
template <>
inline auto PropertyDisplayer::operator()<kuki::BoneData>(kuki::BoneData *boneData) -> void {
  if (!boneData)
    return;
  auto count = boneData->boneCount;
  ImGui::InputInt("Bone Count", &count, 1, 100, ImGuiInputTextFlags_ReadOnly);
}
template <>
inline auto PropertyDisplayer::operator()<kuki::BoundingBox>(kuki::BoundingBox *bounds) -> void {
  if (!bounds)
    return;
  auto boundsMin = bounds->min;
  ImGui::InputFloat3("Min Bounds", glm::value_ptr(boundsMin), nullptr, ImGuiInputTextFlags_ReadOnly);
  auto boundsMax = bounds->max;
  ImGui::InputFloat3("Max Bounds", glm::value_ptr(boundsMax), nullptr, ImGuiInputTextFlags_ReadOnly);
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
  if (ImGui::Checkbox("Active", &makeActive) && makeActive)
    app.SetActiveCamera(entity);
  ImGui::EndDisabled();
  ImGui::SetItemTooltip("Whether this is the camera the scene is drawn through.\nDisabled when it already is: a scene always has one, so the way to\nchange it is to activate another rather than to deactivate this.");
  ImGui::Separator();
  static auto &types = kuki::EnumTraits<kuki::CameraType>::GetNames();
  auto type = static_cast<int>(camera->type);
  if (ImGui::Combo("Type", &type, types.data(), types.size())) {
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
  if (ImGui::DragFloat3("Position", glm::value_ptr(position), .1f)) {
    camera->position = position;
    dirty = true;
  }
  auto rotationDegrees = glm::degrees(glm::eulerAngles(camera->rotation));
  if (ImGui::DragFloat3("Rotation", glm::value_ptr(rotationDegrees), .1f)) {
    camera->rotation = glm::quat(glm::radians(rotationDegrees));
    dirty = true;
  }
  if (camera->type == kuki::CameraType::Perspective) {
    auto fov = camera->fov;
    if (ImGui::SliderFloat("FOV", &fov, .0f, 180.f)) {
      camera->fov = fov;
      dirty = true;
    }
    // Shown, not offered. This follows the render target, and the control that used to sit here
    // took a drag and silently reverted on the next frame because nothing read the result back.
    ImGui::BeginDisabled();
    auto aspectRatio = camera->aspectRatio;
    ImGui::DragFloat("Aspect Ratio", &aspectRatio);
    ImGui::EndDisabled();
    ImGui::SetItemTooltip("Follows the size of the viewport this camera renders into.");
  }
  auto nearPlane = camera->nearPlane;
  if (ImGui::DragFloat("Near Plane", &nearPlane, .1f, .0f, MAX_FLOAT)) {
    camera->nearPlane = nearPlane;
    dirty = true;
  }
  auto farPlane = camera->farPlane;
  if (ImGui::DragFloat("Far Plane", &farPlane, .1f, .0f, MAX_FLOAT)) {
    camera->farPlane = farPlane;
    dirty = true;
  }
  if (camera->type == kuki::CameraType::Orthographic) {
    auto orthoSize = camera->orthoSize;
    if (ImGui::DragFloat("Size", &orthoSize, .1f, .0f, MAX_FLOAT)) {
      camera->orthoSize = orthoSize;
      dirty = true;
    }
  }
  static auto &modes = kuki::EnumTraits<kuki::ExposureMode>::GetNames();
  auto mode = static_cast<int>(camera->exposureMode);
  if (ImGui::Combo("Exposure Mode", &mode, modes.data(), modes.size()))
    camera->exposureMode = static_cast<kuki::ExposureMode>(mode);
  ImGui::SetItemTooltip("Manual sets the scale directly, in stops.\nPhysical derives it from an aperture, shutter and sensitivity, the way a camera does.");
  if (camera->exposureMode == kuki::ExposureMode::Physical) {
    // Written the way the dials on a camera are labelled rather than the way the formula wants
    // them. Aperture is an f-number, so a step towards the wide end halves it and doubles the
    // light; shutter is a reciprocal, since nobody reads 0.008 as a hundred and twenty fifth.
    auto aperture = camera->aperture;
    if (ImGui::DragFloat("Aperture", &aperture, .05f, kuki::MIN_APERTURE, kuki::MAX_APERTURE, "f/%.1f", ImGuiSliderFlags_AlwaysClamp))
      camera->aperture = aperture;
    ImGui::SetItemTooltip("Lower opens the lens and lets in more light.");
    auto denominator = 1.f / std::clamp(camera->shutterSpeed, kuki::MIN_SHUTTER_SPEED, kuki::MAX_SHUTTER_SPEED);
    if (ImGui::DragFloat("Shutter", &denominator, 1.f, 1.f / kuki::MAX_SHUTTER_SPEED, 1.f / kuki::MIN_SHUTTER_SPEED, "1/%.0f s", ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_Logarithmic))
      camera->shutterSpeed = 1.f / std::max(denominator, 1.f / kuki::MAX_SHUTTER_SPEED);
    ImGui::SetItemTooltip("Lower holds the shutter open longer and lets in more light.");
    auto sensitivity = camera->sensitivity;
    if (ImGui::DragFloat("ISO", &sensitivity, 10.f, kuki::MIN_SENSITIVITY, kuki::MAX_SENSITIVITY, "%.0f", ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_Logarithmic))
      camera->sensitivity = sensitivity;
    ImGui::SetItemTooltip("Higher amplifies what the sensor gathered.");
  }
  auto exposure = camera->exposure;
  const auto compensating = camera->exposureMode == kuki::ExposureMode::Physical;
  if (ImGui::SliderFloat(compensating ? "Compensation" : "Exposure", &exposure, -kuki::EXPOSURE_RANGE, kuki::EXPOSURE_RANGE, "%+.2f EV", ImGuiSliderFlags_AlwaysClamp))
    camera->exposure = exposure;
  ImGui::SetItemTooltip("Stops applied on top of whatever the mode produced. One stop doubles the light.");
  // The number both backends actually read, shown because neither mode makes it obvious: in
  // Physical it is several stops away from anything on the dials, and having it on screen is what
  // turns a black frame from a bug into a reading.
  if (compensating) {
    ImGui::TextDisabled("EV100 %.2f, applying %+.2f EV", kuki::ComputeEV100(camera->aperture, camera->shutterSpeed, camera->sensitivity), camera->GetExposureStops());
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(1.f, .8f, .2f, 1.f), "(?)");
    ImGui::SetItemTooltip("Physical exposure assumes lights emit in photometric units, and this engine's do not:\ntheir intensity is a bare multiplier, not lumens or candela. Expect a near black image\nand a large compensation until lighting moves to real units. Manual mode is unaffected.");
  }
  ImGui::Separator();
  // Kept to the bottom of the panel and behind a fold, under everything that describes the camera
  // itself. These do not change what the camera is, only what it draws instead of the picture, and
  // somebody opening this panel is far more often here for the lens than for the diagnostics.
  // The whole fold goes when the backend answers neither, which on the OpenGL one is both of them:
  // its scene shader writes finished pixels and nothing else, and there is no probe field for the
  // spheres to stand for. An empty fold would still read as a feature that had stopped working.
  if ((context.capabilities.lightingDebugViews || context.capabilities.probeVolume) && ImGui::CollapsingHeader("Debug Views")) {
    ImGui::TextDisabled("Not saved with the scene.");
    ImGui::SetItemTooltip("A view is a thing you are in the middle of looking at rather than part of\nthe shot, so it is left out of the scene file deliberately -- reopening a\nscene onto a picture of a weight buffer would look broken, not restored.");
    if (context.capabilities.lightingDebugViews) {
      static auto &lightingNames = kuki::EnumTraits<kuki::LightingDebugView>::GetNames();
      auto lighting = static_cast<int>(camera->lightingDebugView);
      if (ImGui::Combo("Shading", &lighting, lightingNames.data(), lightingNames.size()))
        camera->lightingDebugView = static_cast<kuki::LightingDebugView>(lighting);
      ImGui::SetItemTooltip("One step of the shading, drawn instead of the finished pixel.\nThe bounce, the sky, the direct light and the occlusion are terms;\nthe probe entries are how the field was reconstructed at each point.\nShown without exposure or a tone curve, so a value can be read off the screen.");
    }
    if (context.capabilities.probeVolume) {
      static auto &probeNames = kuki::EnumTraits<kuki::ProbeDebugView>::GetNames();
      auto probe = static_cast<int>(camera->probeDebugView);
      if (ImGui::Combo("Probes", &probe, probeNames.data(), probeNames.size()))
        camera->probeDebugView = static_cast<kuki::ProbeDebugView>(probe);
      ImGui::SetItemTooltip("What the probes are drawn as, over the scene.\nIrradiance is what the field holds; distance and variance are what the\nvisibility test runs on; trust, relocation and classification are\nwhether the mechanisms that maintain the field are working.");
    }
    if (!active)
      ImGui::TextDisabled("Applies when this camera is active.");
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
  ImGui::InputInt("Texture Table", &tableIndex, 0, 0, ImGuiInputTextFlags_ReadOnly);
  ImGui::SetItemTooltip("Index of this material's descriptor table in the bindless heap.");
  ImGui::BeginDisabled();
  auto bound = material->textureTableGPU != 0;
  ImGui::Checkbox("Table Resident", &bound);
  ImGui::EndDisabled();
  ImGui::SetItemTooltip("Whether the table has been built and uploaded yet.");
  DisplayMaterialSurface(material->type, material->fallback);
}
template <>
inline auto PropertyDisplayer::operator()<kuki::GLBuffer>(kuki::GLBuffer *buffer) -> void {
  if (!buffer)
    return;
  auto id = static_cast<int>(buffer->id);
  ImGui::InputInt("Buffer Object", &id, 0, 0, ImGuiInputTextFlags_ReadOnly);
}
template <>
inline auto PropertyDisplayer::operator()<kuki::GLComputeShader>(kuki::GLComputeShader *shader) -> void {
  if (!shader)
    return;
  auto id = static_cast<int>(shader->id);
  ImGui::InputInt("Program", &id, 0, 0, ImGuiInputTextFlags_ReadOnly);
}
template <>
inline auto PropertyDisplayer::operator()<kuki::GLLitShader>(kuki::GLLitShader *shader) -> void {
  if (!shader)
    return;
  auto id = static_cast<int>(shader->id);
  ImGui::InputInt("Program", &id, 0, 0, ImGuiInputTextFlags_ReadOnly);
  static auto &types = kuki::EnumTraits<kuki::MaterialType>::GetNames();
  ImGui::LabelText("Draws", "%s", types[static_cast<size_t>(shader->type)]);
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
  ImGui::InputInt("Vertex Array Object", &vao, 1, 100, ImGuiInputTextFlags_ReadOnly);
  auto ebo = static_cast<int>(mesh->ebo);
  ImGui::InputInt("Element Buffer Object", &ebo, 1, 100, ImGuiInputTextFlags_ReadOnly);
  auto vertexCount = mesh->vertexCount;
  ImGui::InputInt("Vertex Count", &vertexCount, 1, 100, ImGuiInputTextFlags_ReadOnly);
  auto indexCount = mesh->indexCount;
  ImGui::InputInt("Index Count", &indexCount, 1, 100, ImGuiInputTextFlags_ReadOnly);
}
template <>
inline auto PropertyDisplayer::operator()<kuki::GLRenderTarget>(kuki::GLRenderTarget *target) -> void {
  if (!target)
    return;
  ImGui::LabelText("Size", "%d x %d", target->desc.width, target->desc.height);
  auto samples = target->desc.samples;
  ImGui::InputInt("Samples", &samples, 0, 0, ImGuiInputTextFlags_ReadOnly);
  auto framebuffer = static_cast<int>(target->framebuffer);
  ImGui::InputInt("Framebuffer", &framebuffer, 0, 0, ImGuiInputTextFlags_ReadOnly);
  auto renderbuffer = static_cast<int>(target->renderbuffer);
  ImGui::InputInt("Renderbuffer", &renderbuffer, 0, 0, ImGuiInputTextFlags_ReadOnly);
  auto texture = static_cast<int>(target->texture);
  ImGui::InputInt("Colour Texture", &texture, 0, 0, ImGuiInputTextFlags_ReadOnly);
  auto idTexture = static_cast<int>(target->idTexture);
  ImGui::InputInt("Picking Texture", &idTexture, 0, 0, ImGuiInputTextFlags_ReadOnly);
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
  ImGui::InputInt("Program", &id, 0, 0, ImGuiInputTextFlags_ReadOnly);
  static auto &types = kuki::EnumTraits<kuki::MaterialType>::GetNames();
  ImGui::LabelText("Draws", "%s", types[static_cast<size_t>(shader->type)]);
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
  if (ImGui::CollapsingHeader("Terms", ImGuiTreeNodeFlags_DefaultOpen)) {
    if (probes)
      ImGui::TextDisabled("Applied after the field is read. Repaints immediately.");
    else
      ImGui::TextDisabled("This backend has no probe field. Repaints immediately.");
    if (probes) {
      ImGui::SliderFloat("Bounce", &edited.bounceIntensity, .0f, 4.f);
      ImGui::SetItemTooltip("How much of the probe volume's bounce reaches the picture.\nThe first thing to reach for: most of the time the question is\nwhether there is too much indirect light or too little.");
    }
    ImGui::SliderFloat("Sky", &edited.skyIntensity, .0f, 4.f);
    ImGui::SetItemTooltip("The same, for the sky's diffuse contribution. Separate from the\nbounce because the two arrive by different routes, and finding the\nbalance between them needs both.");
    ImGui::SliderFloat("Ambient Fallback", &edited.ambientFallback, .0f, .5f, "%.3f");
    ImGui::SetItemTooltip("The flat ambient a scene falls back on with no sky, no ambient\nlight and no probe volume. Shades nothing when any of the three\nis present.");
  }
  if (probes && ImGui::CollapsingHeader("Reconstruction", ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::TextDisabled("How the field is read at a point. Repaints immediately.");
    ImGui::SliderFloat("Surface Bias", &edited.probeSurfaceBias, .0f, 1.f);
    ImGui::SetItemTooltip("A step off the surface before the field is sampled, as a fraction\nof the local cell. Too small and a concave corner reads as occluded\nfrom every probe around it; too large and the bounce detaches from\nthe geometry, losing the darkening where surfaces meet.");
    ImGui::SliderFloat("Weight Floor", &edited.probeWeightFloor, .0f, 1.f);
    ImGui::SetItemTooltip("Below this a probe the point can barely see is crushed towards\nnothing rather than merely reduced. Raise it to stop light coming\nthrough a thin wall; watch for the probe lattice printing itself\nonto flat surfaces as single corners start deciding whole cells.");
    ImGui::SliderFloat("Visibility Sharpness", &edited.probeVisibilitySharpness, 1.f, 8.f);
    ImGui::SetItemTooltip("How sharply a partly-visible probe is disbelieved. One is the\nstatistical answer and it leaks; higher distrusts a partial verdict\nmore, at the cost of darkening the creases the test is least\ncertain about.");
  }
  if (probes && ImGui::CollapsingHeader("Trace")) {
    ImGui::TextDisabled("Changes what the probes store. Settles over a few dozen frames.");
    ImGui::SliderFloat("Depth Sharpness", &edited.probeDepthSharpness, 4.f, 64.f);
    ImGui::SetItemTooltip("How tightly a ray must line up with a visibility texel to count\ntowards it. Tied to the map's width rather than free: 28 puts the\nlobe's half-width at the angle one texel subtends, and sharpening\nmuch past it leaves texels that no ray feeds.");
    ImGui::SliderFloat("Depth Range", &edited.probeDepthRange, .05f, 2.f);
    ImGui::SetItemTooltip("How far a probe's visibility reaches, as a fraction of the volume's\nside. Sited too close and probes invent an occluder in a ring at\nwhatever radius the clamp begins to bite.");
    ImGui::SliderFloat("Ray Distance", &edited.rayDistanceScale, .25f, 4.f);
    ImGui::SetItemTooltip("How far a ray may travel, as a multiple of the volume's side.\nThe cheapest thing here to shorten: a ray that reaches nothing\ncosts the same as one that does.");
    ImGui::SliderFloat("Hit Normal Nudge", &edited.hitNormalNudge, .0f, 1.f);
    ImGui::SetItemTooltip("How far a hit is nudged along its normal before it is shaded, as a\nfraction of a cell. The trace's equivalent of a shadow bias.");
    ImGui::SliderFloat("Opaque Threshold", &edited.opaqueThreshold, .001f, .5f, "%.3f");
    ImGui::SetItemTooltip("How much light must still be getting through before a ray stops\nbeing followed. Purely a cost control: raising it ends alpha-blended\nchains sooner.");
    auto meanStep = static_cast<int>(edited.meanStep);
    if (ImGui::SliderInt("Mean Step", &meanStep, 1, 16))
      edited.meanStep = static_cast<uint32_t>(std::max(meanStep, 1));
    ImGui::SetItemTooltip("Estimates the running mean is worth, so a fresh one is folded in at\none over n plus this. Lower converges faster and holds more of the\nray set's noise; higher is steadier and slower to notice that the\nlight changed.");
  }
  if (probes && ImGui::CollapsingHeader("Probe Placement")) {
    ImGui::TextDisabled("Where probes stand. Settles over a few dozen frames.");
    ImGui::SliderFloat("Relocation Margin", &edited.relocationMargin, .0f, 1.f);
    ImGui::SetItemTooltip("How far a probe steps towards open space each trace, as a fraction\nof its allowance.");
    ImGui::SliderFloat("Relocation Damping", &edited.relocationDamping, .0f, 1.f);
    ImGui::SetItemTooltip("How much of each step is held back, which is what keeps a probe\nfrom oscillating between two placements.");
    ImGui::SliderFloat("Escape Agreement", &edited.escapeAgreement, .01f, 1.f);
    ImGui::SetItemTooltip("How much the rays coming back off a far side must agree before\ntheir mean is taken for a direction rather than the residue of a\nray set. A probe just outside a surface scores about a half; one\nsealed inside solid geometry, about 0.0016.");
    ImGui::SliderFloat("Buried Limit", &edited.buriedLimit, .01f, 1.f);
    ImGui::SetItemTooltip("What share of a probe's rays may come back off the far side of a\nsurface before it is disbelieved and filled in from its neighbours.\nThe Probe View's Trust and Classification entries show the verdict.");
    ImGui::TextDisabled("Their allowance is fixed when the volume is built.");
    ImGui::SetItemTooltip("PROBE_RELOCATION_LIMIT, baked into each probe's anchor. Changing it\nmeans rebuilding the volume rather than retracing, which is a\ndifferent tier of cost and is not exposed here.");
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
  if (ImGui::Combo("Type", &type, types.data(), types.size()))
    light->type = static_cast<kuki::LightType>(type);
  auto position = light->position;
  if (ImGui::DragFloat3("Position", glm::value_ptr(position), .1f))
    light->position = position;
  auto rotationQuat = light->rotation;
  auto rotationDegrees = glm::degrees(glm::eulerAngles(rotationQuat));
  if (ImGui::DragFloat3("Rotation", glm::value_ptr(rotationDegrees), .1f)) {
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
  if (ImGui::ColorEdit3("Ambient Color", glm::value_ptr(ambient)))
    light->ambient = ambient;
  auto diffuse = light->diffuse;
  if (ImGui::ColorEdit3("Diffuse Color", glm::value_ptr(diffuse)))
    light->diffuse = diffuse;
  auto specular = light->specular;
  if (ImGui::ColorEdit3("Specular Color", glm::value_ptr(specular)))
    light->specular = specular;
  auto intensity = light->intensity;
  if (ImGui::DragFloat("Intensity", &intensity, .05f, .0f, MAX_FLOAT, "%.2f"))
    light->intensity = intensity;
  ImGui::SameLine();
  ImGui::TextDisabled("(?)");
  if (ImGui::IsItemHovered())
    ImGui::SetTooltip("Brightness multiplier applied on top of the diffuse/specular colors above.\nRaise this instead of pushing color channels past 1.0 to make a light brighter.");
  if (light->type == kuki::LightType::Point || light->type == kuki::LightType::Spot) {
    constexpr auto MIN_RANGE = 1.f;
    const auto currentLinear = std::max(light->linear, 1.0e-4f);
    const auto currentRange = 4.5f / currentLinear;
    auto range = currentRange;
    if (ImGui::DragFloat("Range", &range, .5f, MIN_RANGE, MAX_FLOAT, "%.1f")) {
      range = std::max(range, MIN_RANGE);
      light->constant = 1.f;
      light->linear = 4.5f / range;
      light->quadratic = 75.f / (range * range);
    }
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered())
      ImGui::SetTooltip("Approximate distance the light reaches before fading out.\nReplaces manually tuning constant/linear/quadratic attenuation terms.");
    // The three terms Range drives, shown alongside it rather than behind a fold. Editing one by
    // hand puts the falloff somewhere Range cannot express, and the next drag of Range overwrites
    // all three; that is the intended relationship, and it is easier to see when both are visible.
    auto constant = light->constant;
    if (ImGui::SliderFloat("Constant Term", &constant, .0f, 1.f))
      light->constant = constant;
    auto linear = light->linear;
    if (ImGui::SliderFloat("Linear Term", &linear, .0f, 1.f))
      light->linear = linear;
    auto quadratic = light->quadratic;
    if (ImGui::SliderFloat("Quadratic Term", &quadratic, .0f, 1.f))
      light->quadratic = quadratic;
  }
  if (light->type == kuki::LightType::Spot) {
    auto innerAngle = glm::degrees(std::acos(std::clamp(light->innerCutoff, -1.f, 1.f)));
    auto outerAngle = glm::degrees(std::acos(std::clamp(light->outerCutoff, -1.f, 1.f)));
    if (ImGui::DragFloat("Inner Cone Angle", &innerAngle, .1f, .0f, outerAngle))
      light->innerCutoff = std::cos(glm::radians(innerAngle));
    if (ImGui::DragFloat("Outer Cone Angle", &outerAngle, .1f, innerAngle, 89.f))
      light->outerCutoff = std::cos(glm::radians(outerAngle));
    auto spotNearPlane = light->nearPlane;
    if (ImGui::DragFloat("Shadow Near Plane", &spotNearPlane, .1f, .0f, MAX_FLOAT))
      light->nearPlane = spotNearPlane;
    auto spotFarPlane = light->farPlane;
    if (ImGui::DragFloat("Shadow Far Plane", &spotFarPlane, .1f, .0f, MAX_FLOAT))
      light->farPlane = spotFarPlane;
  }
  if (light->type == kuki::LightType::Directional) {
    auto nearPlane = light->nearPlane;
    if (ImGui::DragFloat("Shadow Near Plane", &nearPlane, .1f, .0f, MAX_FLOAT))
      light->nearPlane = nearPlane;
    auto farPlane = light->farPlane;
    if (ImGui::DragFloat("Shadow Far Plane", &farPlane, .1f, .0f, MAX_FLOAT))
      light->farPlane = farPlane;
    auto orthoSize = light->orthoSize;
    if (ImGui::DragFloat("Shadow Coverage Size", &orthoSize, .1f, .0f, MAX_FLOAT))
      light->orthoSize = orthoSize;
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered())
      ImGui::SetTooltip("Half-width of the area around this light that the shadow map covers.\nIncrease if shadows clip or disappear near the edges of the scene.");
  }
}
template <>
inline auto PropertyDisplayer::operator()<kuki::MaterialHandle>(kuki::MaterialHandle *handle) -> void {
  if (!handle)
    return;
  auto assetId = handle->assetId;
  ImGui::InputScalar("Asset ID", ImGuiDataType_U64, &assetId, nullptr, nullptr, nullptr, ImGuiInputTextFlags_ReadOnly);
}
template <>
inline auto PropertyDisplayer::operator()<kuki::MeshHandle>(kuki::MeshHandle *handle) -> void {
  if (!handle)
    return;
  auto assetId = handle->assetId;
  ImGui::InputScalar("Asset ID", ImGuiDataType_U64, &assetId, nullptr, nullptr, nullptr, ImGuiInputTextFlags_ReadOnly);
}
template <>
inline auto PropertyDisplayer::operator()<kuki::ModelMaterialHandle>(kuki::ModelMaterialHandle *handle) -> void {
  if (!handle)
    return;
  auto modelAssetId = handle->modelAssetId;
  ImGui::InputScalar("Model Asset ID", ImGuiDataType_U64, &modelAssetId, nullptr, nullptr, nullptr, ImGuiInputTextFlags_ReadOnly);
  auto materialIndex = handle->materialIndex;
  ImGui::InputScalar("Material Index", ImGuiDataType_U64, &materialIndex, nullptr, nullptr, nullptr, ImGuiInputTextFlags_ReadOnly);
}
template <>
inline auto PropertyDisplayer::operator()<kuki::ModelMeshHandle>(kuki::ModelMeshHandle *handle) -> void {
  if (!handle)
    return;
  auto modelAssetId = handle->modelAssetId;
  ImGui::InputScalar("Model Asset ID", ImGuiDataType_U64, &modelAssetId, nullptr, nullptr, nullptr, ImGuiInputTextFlags_ReadOnly);
  auto meshIndex = handle->meshIndex;
  ImGui::InputScalar("Mesh Index", ImGuiDataType_U64, &meshIndex, nullptr, nullptr, nullptr, ImGuiInputTextFlags_ReadOnly);
}
template <>
inline auto PropertyDisplayer::operator()<kuki::Script>(kuki::Script *script) -> void {
  if (!script)
    return;
  script->Display();
}
template <>
inline auto PropertyDisplayer::operator()<kuki::Skeleton>(kuki::Skeleton *skeleton) -> void {
  if (!skeleton)
    return;
  auto nodeCount = static_cast<int>(skeleton->nodeEntities.size());
  ImGui::InputInt("Node Count", &nodeCount, 0, 0, ImGuiInputTextFlags_ReadOnly);
  // A skeleton is a flat array indexed by the animation channels, and a hole in it means a channel
  // that will animate nothing. Counting the holes is the part worth surfacing: the array's length
  // alone looks healthy right up until the moment a clip drives a bone that was never bound.
  const auto bound = std::count_if(skeleton->nodeEntities.begin(), skeleton->nodeEntities.end(), [](const kuki::EntityID id) { return static_cast<bool>(id); });
  const auto missing = static_cast<int>(skeleton->nodeEntities.size()) - static_cast<int>(bound);
  if (missing > 0)
    ImGui::TextColored(ImVec4(1.f, .8f, .2f, 1.f), "%d node(s) not bound to an entity", missing);
  else if (nodeCount > 0)
    ImGui::TextDisabled("Every node is bound.");
}
template <>
inline auto PropertyDisplayer::operator()<kuki::SkyboxHandle>(kuki::SkyboxHandle *handle) -> void {
  if (!handle)
    return;
  auto assetId = handle->assetId;
  ImGui::InputScalar("Asset ID", ImGuiDataType_U64, &assetId, nullptr, nullptr, nullptr, ImGuiInputTextFlags_ReadOnly);
}
template <>
inline auto PropertyDisplayer::operator()<kuki::TextureHandle>(kuki::TextureHandle *handle) -> void {
  if (!handle)
    return;
  auto assetId = handle->assetId;
  ImGui::InputScalar("Asset ID", ImGuiDataType_U64, &assetId, nullptr, nullptr, nullptr, ImGuiInputTextFlags_ReadOnly);
}
template <>
inline auto PropertyDisplayer::operator()<kuki::Transform>(kuki::Transform *transform) -> void {
  static constexpr auto MAX_FLOAT = std::numeric_limits<float>::max();
  if (!transform)
    return;
  auto dirty = false;
  auto position = transform->position;
  if (ImGui::DragFloat3("Position", glm::value_ptr(position), .1f)) {
    transform->position = position;
    dirty = true;
  }
  auto rotationQuat = transform->rotation;
  auto rotationDegrees = glm::degrees(glm::eulerAngles(rotationQuat));
  if (ImGui::DragFloat3("Rotation", glm::value_ptr(rotationDegrees), .1f)) {
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
    if (ImGui::DragFloat("Scale", &uniformScale, .1f, .0f, MAX_FLOAT)) {
      transform->scale = glm::vec3(uniformScale);
      dirty = true;
    }
    ImGui::SameLine();
    ImGui::Checkbox("Uniform", &uniformMode);
  } else {
    if (ImGui::DragFloat3("Scale", glm::value_ptr(scale), .1f, .0f, MAX_FLOAT)) {
      transform->scale = scale;
      dirty = true;
    }
    ImGui::SameLine();
    ImGui::Checkbox("Uniform", &uniformMode);
  }
  if (dirty)
    app.MarkTransformDirty(context.selectedEntityId);
}
