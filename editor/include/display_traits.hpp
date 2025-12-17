#pragma once
#ifndef GLM_ENABLE_EXPERIMENTAL
#define GLM_ENABLE_EXPERIMENTAL
#endif
#include <camera.hpp>
#include <component.hpp>
#include <component_traits.hpp>
#include <editor.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/trigonometric.hpp>
#include <imgui.h>
#include <light.hpp>
#include <material.hpp>
#include <mesh_filter.hpp>
#include <mesh_renderer.hpp>
#include <skybox.hpp>
#include <string>
#include <texture.hpp>
#include <variant>
using namespace kuki;
template <typename T>
struct DisplayTraits {
  static_assert(sizeof(T) == 0, "DisplayTraits must be specialized for this type.");
  static const std::string GetName();
  static void DisplayProperties(T*, EditorContext&, int&);
};
template <>
struct DisplayTraits<BoneData> {
  static const std::string GetName() {
    return "BoneData";
  }
  static void DisplayProperties(BoneData* boneData, EditorContext& context) {
    constexpr auto MAX_FLOAT = std::numeric_limits<float>::max();
    if (!boneData)
      return;
    auto ssbo = boneData->boneSSBO;
    if (ImGui::InputInt("SSBO", &ssbo))
      boneData->boneSSBO = ssbo;
    auto count = boneData->boneCount;
    if (ImGui::InputInt("Bone Count", &count))
      boneData->boneCount = count;
  }
};
template <>
struct DisplayTraits<Camera> {
  static const std::string GetName() {
    return "Camera";
  }
  static void DisplayProperties(Camera* camera, EditorContext& context) {
    constexpr auto MAX_FLOAT = std::numeric_limits<float>::max();
    if (!camera)
      return;
    auto settingsDirty = false;
    static auto& types = EnumTraits<CameraType>::GetNames();
    auto type = static_cast<int>(camera->type);
    if (ImGui::Combo("Type", &type, types.data(), types.size())) {
      camera->type = static_cast<CameraType>(type);
      settingsDirty = true;
    }
    auto rotationDirty = false;
    auto rotationQuat = camera->rotation;
    auto rotationDegrees = glm::degrees(glm::eulerAngles(rotationQuat));
    if (ImGui::DragFloat3("Rotation", glm::value_ptr(rotationDegrees), .1f)) {
      for (auto i = 0; i < 3; ++i) {
        auto& angle = rotationDegrees[i];
        while (angle > 180.0f)
          angle -= 360.0f;
        while (angle < -180.0f)
          angle += 360.0f;
      }
      auto rotationRadians = glm::radians(rotationDegrees);
      camera->rotation = glm::quat(rotationRadians);
      rotationDirty = true;
    }
    if (camera->type == CameraType::Perspective) {
      auto fov = camera->fov;
      if (ImGui::SliderFloat("FOV", &fov, .0f, 180.0f)) {
        camera->fov = fov;
        settingsDirty = true;
      }
      auto aspectRatio = camera->aspectRatio;
      if (ImGui::SliderFloat("Aspect Ratio", &aspectRatio, .1f, 10.0f)) {
        camera->aspectRatio = aspectRatio;
        settingsDirty = true;
      }
    }
    auto nearPlane = camera->nearPlane;
    if (ImGui::DragFloat("Near Plane", &nearPlane, .1f, .0f, MAX_FLOAT)) {
      camera->nearPlane = nearPlane;
      settingsDirty = true;
    }
    auto farPlane = camera->farPlane;
    if (ImGui::DragFloat("Far Plane", &farPlane, .1f, .0f, MAX_FLOAT)) {
      camera->farPlane = farPlane;
      settingsDirty = true;
    }
    if (camera->type == CameraType::Orthographic) {
      auto orthoSize = camera->orthoSize;
      if (ImGui::DragFloat("Size", &orthoSize, .1f, .0f, MAX_FLOAT)) {
        camera->orthoSize = orthoSize;
        settingsDirty = true;
      }
    }
    auto positionDirty = false;
    auto position = camera->position;
    if (ImGui::DragFloat3("Position", glm::value_ptr(position), .1f)) {
      camera->position = position;
      positionDirty = true;
    }
    camera->positionDirty |= positionDirty;
    camera->rotationDirty |= rotationDirty;
    camera->settingsDirty |= settingsDirty;
    camera->uboDirty |= positionDirty || rotationDirty || settingsDirty;
  }
};
template <>
struct DisplayTraits<Light> {
  static const std::string GetName() {
    return "Light";
  }
  static void DisplayProperties(Light* light, EditorContext& context) {
    if (!light)
      return;
    static auto& types = EnumTraits<LightType>::GetNames();
    auto type = static_cast<int>(light->type);
    if (ImGui::Combo("Type", &type, types.data(), types.size()))
      light->type = static_cast<LightType>(type);
    auto vector = light->vector;
    if (ImGui::DragFloat3(light->type == LightType::Directional ? "Direction" : "Position", glm::value_ptr(vector), .1f))
      light->vector = vector;
    auto ambient = light->ambient;
    if (ImGui::ColorEdit3("Ambient Color", glm::value_ptr(ambient)))
      light->ambient = ambient;
    auto diffuse = light->diffuse;
    if (ImGui::ColorEdit3("Diffuse Color", glm::value_ptr(diffuse)))
      light->diffuse = diffuse;
    auto specular = light->specular;
    if (ImGui::ColorEdit3("Specular Color", glm::value_ptr(specular)))
      light->specular = specular;
    if (light->type == LightType::Point) {
      // TODO: expose these in a more user-friendly fashion
      auto constant = light->constant;
      if (ImGui::SliderFloat("Constant Term", &constant, .0f, 1.0f))
        light->constant = constant;
      auto linear = light->linear;
      if (ImGui::SliderFloat("Linear Term", &linear, .0f, 1.0f))
        light->linear = linear;
      auto quadratic = light->quadratic;
      if (ImGui::SliderFloat("Quadratic Term", &quadratic, .0f, 1.0f))
        light->quadratic = quadratic;
    }
  }
};
template <>
struct DisplayTraits<LitMaterial> {
  static const std::string GetName() {
    return "Lit Material";
  }
  static void DisplayProperties(LitMaterial* material, EditorContext& context) {
    constexpr ImVec2 TEXTURE_SIZE(64, 64);
    if (!material)
      return;
    const auto& style = ImGui::GetStyle();
    auto GetTileWidth = [&](const char* label) -> float {
      const auto textWidth = ImGui::CalcTextSize(label).x;
      const auto buttonWidth = TEXTURE_SIZE.x + style.FramePadding.x * 2.0f;
      return (std::max)(buttonWidth, textWidth);
    };
    auto DrawTextureTile = [&](const char* id, ImTextureID tex, const char* label, auto onClick) -> float {
      const auto tileWidth = GetTileWidth(label);
      auto startPos = ImGui::GetCursorPos();
      ImGui::BeginGroup();
      auto clicked = ImGui::ImageButton(id, tex, TEXTURE_SIZE);
      if (clicked)
        onClick();
      const auto textWidth = ImGui::CalcTextSize(label).x;
      const auto buttonHeight = TEXTURE_SIZE.y + style.FramePadding.y * 2.0f;
      ImGui::SetCursorPosX(startPos.x + (tileWidth - textWidth) * .5f);
      ImGui::SetCursorPosY(startPos.y + buttonHeight + style.ItemInnerSpacing.y);
      ImGui::TextUnformatted(label);
      ImGui::EndGroup();
      return tileWidth;
    };
    auto SelectProperty = [&](MaterialProperty prop) {
      context.selectedComponent = static_cast<int>(ComponentType::Texture);
      context.assetMask = static_cast<int>(ComponentMask::Texture);
      context.selectedProperty = static_cast<int>(prop);
    };
    struct TexEntry {
      const char* id;
      ImTextureID tex;
      const char* label;
      MaterialProperty prop;
    };
    TexEntry entries[] = {
      {"Albedo", static_cast<ImTextureID>(material->data.albedo), "Albedo", MaterialProperty::AlbedoTexture},
      {"Normal", static_cast<ImTextureID>(material->data.normal), "Normal", MaterialProperty::NormalTexture},
      {"Metalness", static_cast<ImTextureID>(material->data.metalness), "Metalness", MaterialProperty::MetalnessTexture},
      {"Occlusion", static_cast<ImTextureID>(material->data.occlusion), "Occlusion", MaterialProperty::OcclusionTexture},
      {"Roughness", static_cast<ImTextureID>(material->data.roughness), "Roughness", MaterialProperty::RoughnessTexture},
      {"Specular", static_cast<ImTextureID>(material->data.specular), "Specular", MaterialProperty::SpecularTexture},
      {"Emissive", static_cast<ImTextureID>(material->data.emissive), "Emissive", MaterialProperty::EmissiveTexture},
    };
    auto firstOnLine = true;
    auto lineWidth = .0f;
    auto remaining = .0f;
    for (const auto& e : entries) {
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
    int mask = material->fallback.textureMask;
    if (ImGui::InputInt("Texture Mask", &mask))
      material->fallback.textureMask = mask;
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
    if (ImGui::SliderFloat("Metalness Factor", &metalnessFactor, 0.0f, 1.0f))
      material->fallback.metalness = metalnessFactor;
    auto occlusionFactor = material->fallback.occlusion;
    if (ImGui::SliderFloat("Occlusion Factor", &occlusionFactor, 0.0f, 1.0f))
      material->fallback.occlusion = occlusionFactor;
    auto roughnessFactor = material->fallback.roughness;
    if (ImGui::SliderFloat("Roughness Factor", &roughnessFactor, 0.0f, 1.0f))
      material->fallback.roughness = roughnessFactor;
  }
};
template <>
struct DisplayTraits<UnlitMaterial> {
  static const std::string GetName() {
    return "Unlit Material";
  }
  static void DisplayProperties(UnlitMaterial* material, EditorContext& context) {
    constexpr auto TEXTURE_SIZE = ImVec2(64, 64);
    if (!material)
      return;
    auto base = material->data.base;
    // if (ImGui::ImageButton("Base Texture", base, TEXTURE_SIZE)) {}
    auto mask = material->fallback.textureMask;
    if (ImGui::InputInt("Texture Mask", &mask))
      material->fallback.textureMask = mask;
    auto baseColor = material->fallback.base;
    if (ImGui::ColorEdit4("Base Color", glm::value_ptr(baseColor)))
      material->fallback.base = baseColor;
  }
};
template <>
struct DisplayTraits<Material> {
  static const std::string GetName() {
    return "Material";
  }
  static void DisplayProperties(Material* material, EditorContext& context) {
    if (!material)
      return;
    auto types = EnumTraits<MaterialType>::GetNames();
    auto type = static_cast<int>(material->GetType());
    if (ImGui::Combo("Type", &type, types.data(), static_cast<int>(types.size())))
      material->SetType(static_cast<MaterialType>(type));
    if (auto litMaterial = std::get_if<LitMaterial>(&material->current))
      DisplayTraits<LitMaterial>::DisplayProperties(litMaterial, context);
    else if (auto unlitMaterial = std::get_if<UnlitMaterial>(&material->current))
      DisplayTraits<UnlitMaterial>::DisplayProperties(unlitMaterial, context);
  }
};
template <>
struct DisplayTraits<Mesh> {
  static const std::string GetName() {
    return "Mesh";
  }
  static void DisplayProperties(Mesh* mesh, EditorContext& context) {
    if (!mesh)
      return;
    auto vao = mesh->vao;
    if (ImGui::InputInt("Vertex Array Object", &vao))
      mesh->vao = vao;
    auto ebo = mesh->ebo;
    if (ImGui::InputInt("Element Buffer Object", &ebo))
      mesh->ebo = ebo;
    auto vertexCount = mesh->vertexCount;
    if (ImGui::InputInt("Vertex Count", &vertexCount))
      mesh->vertexCount = vertexCount;
    auto indexCount = mesh->indexCount;
    if (ImGui::InputInt("Index Count", &indexCount))
      mesh->indexCount = indexCount;
    auto minBounds = mesh->bounds.min;
    if (ImGui::InputFloat3("Minimum Bounds", glm::value_ptr(minBounds)))
      mesh->bounds.min = minBounds;
    auto maxBounds = mesh->bounds.max;
    if (ImGui::InputFloat3("Maximum Bounds", glm::value_ptr(maxBounds)))
      mesh->bounds.max = maxBounds;
  }
};
template <>
struct DisplayTraits<MeshFilter> {
  static const std::string GetName() {
    return "Mesh Filter";
  }
  static void DisplayProperties(MeshFilter* filter, EditorContext& context) {
    if (!filter)
      return;
    DisplayTraits<Mesh>::DisplayProperties(&filter->mesh, context);
  }
};
template <>
struct DisplayTraits<MeshRenderer> {
  static const std::string GetName() {
    return "Mesh Renderer";
  }
  static void DisplayProperties(MeshRenderer* renderer, EditorContext& context) {
    if (!renderer)
      return;
    DisplayTraits<Material>::DisplayProperties(&renderer->material, context);
  }
};
template <>
struct DisplayTraits<Skybox> {
  static const std::string GetName() {
    return "Skybox";
  }
  static void DisplayProperties(Skybox* skybox, EditorContext& context) {
    constexpr ImVec2 TEXTURE_SIZE(64, 64);
    if (!skybox)
      return;
    const auto& style = ImGui::GetStyle();
    auto GetTileWidth = [&](const char* label) -> float {
      const auto textWidth = ImGui::CalcTextSize(label).x;
      const auto buttonWidth = TEXTURE_SIZE.x + style.FramePadding.x * 2.0f;
      return (std::max)(buttonWidth, textWidth);
    };
    auto DrawTextureTile = [&](const char* id, ImTextureID tex, const char* label, auto onClick) -> float {
      const auto tileWidth = GetTileWidth(label);
      auto start_pos = ImGui::GetCursorPos();
      ImGui::BeginGroup();
      auto clicked = ImGui::ImageButton(id, tex, TEXTURE_SIZE);
      if (clicked)
        onClick();
      const auto textWidth = ImGui::CalcTextSize(label).x;
      const auto buttonHeight = TEXTURE_SIZE.y + style.FramePadding.y * 2.0f;
      ImGui::SetCursorPosX(start_pos.x + (tileWidth - textWidth) * .5f);
      ImGui::SetCursorPosY(start_pos.y + buttonHeight + style.ItemInnerSpacing.y);
      ImGui::TextUnformatted(label);
      ImGui::EndGroup();
      return tileWidth;
    };
    struct TexEntry {
      const char* id;
      ImTextureID tex;
      const char* label;
      std::function<void()> onClick;
    };
    TexEntry entries[] = {
      // NOTE: a preview is displayed instead of the skybox which is a cubemap
      {"Skybox##Texture", static_cast<ImTextureID>(skybox->preview), "Skybox", [&context] {
	context.selectedComponent = static_cast<int>(ComponentType::Skybox);
        context.assetMask = static_cast<int>(ComponentMask::Skybox); }},
      // {"Irradiance", static_cast<ImTextureID>(skybox->irradiance), "Irradiance", [] {}},
      // {"Prefilter", static_cast<ImTextureID>(skybox->prefilter), "Prefilter", [] {}},
      // {"BRDF", static_cast<ImTextureID>(skybox->brdf), "BRDF", [] {}},
      // {"Preview", static_cast<ImTextureID>(skybox->preview), "Preview", [] {}}
    };
    auto firstOnLine = true;
    auto remaining = .0f;
    for (const auto& e : entries) {
      if (firstOnLine)
        remaining = ImGui::GetContentRegionAvail().x;
      const auto tileWidth = GetTileWidth(e.label);
      if (!firstOnLine) {
        if (tileWidth + style.ItemSpacing.x <= remaining) {
          ImGui::SameLine();
          remaining -= style.ItemSpacing.x;
        } else {
          firstOnLine = true;
          remaining = ImGui::GetContentRegionAvail().x;
        }
      }
      remaining -= DrawTextureTile(e.id, e.tex, e.label, e.onClick);
      firstOnLine = false;
    }
  }
};
template <>
struct DisplayTraits<Texture> {
  static const std::string GetName() {
    return "Texture";
  }
  static void DisplayProperties(Texture* texture, EditorContext& context) {
    constexpr auto MAX_INT = std::numeric_limits<int>::max();
    constexpr auto TEXTURE_SIZE = ImVec2(64, 64);
    if (!texture)
      return;
    auto types = EnumTraits<TextureType>::GetNames();
    auto type = static_cast<int>(texture->type);
    if (ImGui::Combo("Type", &type, types.data(), types.size()))
      texture->type = static_cast<TextureType>(type);
    auto textureId = texture->id;
    if (ImGui::ImageButton("ID", textureId, TEXTURE_SIZE)) {}
    auto width = texture->width;
    if (ImGui::DragInt("Width", &width, .1f, 0, MAX_INT))
      texture->width = width;
    auto height = texture->height;
    if (ImGui::DragInt("Height", &height, .1f, 0, MAX_INT))
      texture->height = height;
  }
};
template <>
struct DisplayTraits<Transform> {
  static const std::string GetName() {
    return "Transform";
  }
  static void DisplayProperties(Transform* transform, EditorContext& context) {
    constexpr auto MAX_FLOAT = std::numeric_limits<float>::max();
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
        auto& angle = rotationDegrees[i];
        while (angle > 180.0f)
          angle -= 360.0f;
        while (angle < -180.0f)
          angle += 360.0f;
      }
      auto rotationRadians = glm::radians(rotationDegrees);
      transform->rotation = glm::quat(rotationRadians);
      dirty = true;
    }
    auto scale = transform->scale;
    static auto uniformMode = false;
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
};
