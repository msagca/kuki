#pragma once
#include <application.hpp>
#include <array>
#include <bone_data.hpp>
#include <bounding_box.hpp>
#include <camera.hpp>
#include <component_type.hpp>
#include <enum_traits.hpp>
#include <gl_material.hpp>
#include <gl_mesh.hpp>
#include <gl_skybox.hpp>
#include <gl_texture.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>
#include <light.hpp>
#include <material_type.hpp>
#include <mesh_handle.hpp>
#include <skybox_handle.hpp>
#include <texture_handle.hpp>
struct PropertyDisplayer {
  template <typename T>
  auto operator()(T *) -> void;
};
template <typename T>
inline auto PropertyDisplayer::operator()(T *) -> void {}
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
  static auto &types = kuki::EnumTraits<kuki::CameraType>::GetNames();
  auto type = static_cast<int>(camera->type);
  if (ImGui::Combo("Type", &type, types.data(), types.size())) {
    camera->type = static_cast<kuki::CameraType>(type);
    dirty = true;
  }
  auto rotationQuat = camera->rotation;
  auto rotationDegrees = glm::degrees(glm::eulerAngles(rotationQuat));
  if (ImGui::DragFloat3("Rotation", glm::value_ptr(rotationDegrees), .1f)) {
    for (auto i = 0; i < 3; ++i) {
      auto angle = rotationDegrees[i];
      while (angle > 180.f)
        angle -= 360.f;
      while (angle < -180.f)
        angle += 360.f;
    }
    camera->rotation = glm::quat(glm::radians(rotationDegrees));
    dirty = true;
  }
  if (camera->type == kuki::CameraType::Perspective) {
    auto fov = camera->fov;
    if (ImGui::SliderFloat("FOV", &fov, .0f, 180.f)) {
      camera->fov = fov;
      dirty = true;
    }
    auto aspectRatio = camera->aspectRatio;
    if (ImGui::SliderFloat("Aspect Ratio", &aspectRatio, .1f, 10.f, nullptr, ImGuiInputTextFlags_ReadOnly)) {
      // camera->aspectRatio = aspectRatio;
      // dirty = true;
    }
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
  auto position = camera->position;
  if (ImGui::DragFloat3("Position", glm::value_ptr(position), .1f)) {
    camera->position = position;
    dirty = true;
  }
  camera->dirty += dirty;
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
  auto DrawTextureTile = [&](const char *id, ImTextureID tex, const char *label, auto onClick) -> float {
    const auto tileWidth = GetTileWidth(label);
    auto startPos = ImGui::GetCursorPos();
    ImGui::BeginGroup();
    auto clicked = ImGui::ImageButton(id, tex, TEXTURE_SIZE);
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
  const auto SelectProperty = [&](const kuki::MaterialProperty prop) {};
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
    remaining -= DrawTextureTile(e.id, e.tex, e.label, [&SelectProperty, &e] { SelectProperty(e.prop); });
    firstOnLine = false;
  }
  static auto &types = kuki::EnumTraits<kuki::MaterialType>::GetNames();
  auto type = static_cast<int>(material->type);
  if (ImGui::Combo("Type", &type, types.data(), types.size()))
    material->type = static_cast<kuki::MaterialType>(type);
  auto albedoColor = material->fallback.albedo;
  if (ImGui::ColorEdit4("Albedo Color", glm::value_ptr(albedoColor)))
    material->fallback.albedo = albedoColor;
  auto specularColor = material->fallback.specular;
  if (ImGui::ColorEdit4("Specular Color", glm::value_ptr(specularColor)))
    material->fallback.specular = specularColor;
  auto emissiveColor = material->fallback.emissive;
  if (ImGui::ColorEdit4("Emissive Color", glm::value_ptr(emissiveColor)))
    material->fallback.emissive = emissiveColor;
  auto metalnessFactor = material->fallback.metalness;
  if (ImGui::SliderFloat("Metalness Factor", &metalnessFactor, .0f, 1.f))
    material->fallback.metalness = metalnessFactor;
  auto occlusionFactor = material->fallback.occlusion;
  if (ImGui::SliderFloat("Occlusion Factor", &occlusionFactor, .0f, 1.f))
    material->fallback.occlusion = occlusionFactor;
  auto roughnessFactor = material->fallback.roughness;
  if (ImGui::SliderFloat("Roughness Factor", &roughnessFactor, .0f, 1.f))
    material->fallback.roughness = roughnessFactor;
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
inline auto PropertyDisplayer::operator()<kuki::GLSkybox>(kuki::GLSkybox *skybox) -> void {
  static constexpr ImVec2 TEXTURE_SIZE(64, 64);
  static constexpr ImVec2 UV0(0.f, 1.f);
  static constexpr ImVec2 UV1(1.f, 0.f);
  if (!skybox)
    return;
  const auto &style = ImGui::GetStyle();
  const auto label = "Skybox";
  // FIXME: create a preview image for the skybox
  auto tex = static_cast<ImTextureID>(skybox->prefilter);
  const float textWidth = ImGui::CalcTextSize(label).x;
  const float buttonWidth = TEXTURE_SIZE.x + style.FramePadding.x * 2.f;
  const float tileWidth = std::max(buttonWidth, textWidth);
  auto startPos = ImGui::GetCursorPos();
  ImGui::BeginGroup();
  if (ImGui::ImageButton("Skybox##Texture", tex, TEXTURE_SIZE, UV0, UV1)) {}
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
  auto textureId = texture->id;
  if (ImGui::ImageButton("ID", textureId, TEXTURE_SIZE)) {}
}
template <>
inline auto PropertyDisplayer::operator()<kuki::Light>(kuki::Light *light) -> void {
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
    light->rotation = glm::quat(rotationRadians);
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
  if (light->type == kuki::LightType::Point || light->type == kuki::LightType::Spot) {
    // TODO: expose these in a more user-friendly fashion
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
    auto innerCutoff = light->innerCutoff;
    if (ImGui::SliderFloat("Cos(Inner Cut-off Angle)", &innerCutoff, .0f, 1.f))
      light->innerCutoff = innerCutoff;
    auto outerCutoff = light->outerCutoff;
    if (ImGui::SliderFloat("Cos(Outer Cut-off Angle)", &outerCutoff, .0f, 1.f))
      light->outerCutoff = outerCutoff;
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
inline auto PropertyDisplayer::operator()<kuki::SceneMaterialHandle>(kuki::SceneMaterialHandle *handle) -> void {
  if (!handle)
    return;
  auto sceneAssetId = handle->sceneAssetId;
  ImGui::InputScalar("Scene Asset ID", ImGuiDataType_U64, &sceneAssetId, nullptr, nullptr, nullptr, ImGuiInputTextFlags_ReadOnly);
  auto materialIndex = handle->materialIndex;
  ImGui::InputScalar("Material Index", ImGuiDataType_U64, &materialIndex, nullptr, nullptr, nullptr, ImGuiInputTextFlags_ReadOnly);
}
template <>
inline auto PropertyDisplayer::operator()<kuki::SceneMeshHandle>(kuki::SceneMeshHandle *handle) -> void {
  if (!handle)
    return;
  auto sceneAssetId = handle->sceneAssetId;
  ImGui::InputScalar("Scene Asset ID", ImGuiDataType_U64, &sceneAssetId, nullptr, nullptr, nullptr, ImGuiInputTextFlags_ReadOnly);
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
  transform->dirty |= dirty;
}
