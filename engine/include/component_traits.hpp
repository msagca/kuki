#pragma once
#include <bone_data.hpp>
#include <camera.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/ext/vector_float4.hpp>
#include <kuki_engine_export.h>
#include <light.hpp>
#include <mesh.hpp>
#include <mesh_filter.hpp>
#include <mesh_renderer.hpp>
#include <skybox.hpp>
#include <string>
#include <texture.hpp>
#include <transform.hpp>
namespace kuki {
enum class ComponentType : uint8_t {
  BoneData,
  Camera,
  Light,
  Material,
  MeshFilter,
  Mesh,
  MeshRenderer,
  Script,
  Skybox,
  Texture,
  Transform,
  Unknown
};
enum class ComponentMask : size_t {
  BoneData = static_cast<size_t>(1) << static_cast<uint8_t>(ComponentType::BoneData),
  Camera = static_cast<size_t>(1) << static_cast<uint8_t>(ComponentType::Camera),
  Light = static_cast<size_t>(1) << static_cast<uint8_t>(ComponentType::Light),
  Material = static_cast<size_t>(1) << static_cast<uint8_t>(ComponentType::Material),
  MeshFilter = static_cast<size_t>(1) << static_cast<uint8_t>(ComponentType::MeshFilter),
  Mesh = static_cast<size_t>(1) << static_cast<uint8_t>(ComponentType::Mesh),
  MeshRenderer = static_cast<size_t>(1) << static_cast<uint8_t>(ComponentType::MeshRenderer),
  Script = static_cast<size_t>(1) << static_cast<uint8_t>(ComponentType::Script),
  Skybox = static_cast<size_t>(1) << static_cast<uint8_t>(ComponentType::Skybox),
  Texture = static_cast<size_t>(1) << static_cast<uint8_t>(ComponentType::Texture),
  Transform = static_cast<size_t>(1) << static_cast<uint8_t>(ComponentType::Transform)
};
template <typename T>
struct ComponentTraits {
  static_assert(sizeof(T) == 0, "ComponentTraits must be specialized for this type.");
  static const std::string GetName();
  static ComponentType GetType();
  static ComponentMask GetMask();
};
template <>
struct ComponentTraits<BoneData> {
  static const std::string GetName() {
    return "BoneData";
  }
  static ComponentType GetType() {
    return ComponentType::BoneData;
  }
  static ComponentMask GetMask() {
    return ComponentMask::BoneData;
  }
};
template <>
struct ComponentTraits<Camera> {
  static const std::string GetName() {
    return "Camera";
  }
  static ComponentType GetType() {
    return ComponentType::Camera;
  }
  static ComponentMask GetMask() {
    return ComponentMask::Camera;
  }
};
template <>
struct ComponentTraits<Light> {
  static const std::string GetName() {
    return "Light";
  }
  static ComponentType GetType() {
    return ComponentType::Light;
  }
  static ComponentMask GetMask() {
    return ComponentMask::Light;
  }
};
template <>
struct ComponentTraits<Material> {
  static const std::string GetName() {
    return "Material";
  }
  static ComponentType GetType() {
    return ComponentType::Material;
  }
  static ComponentMask GetMask() {
    return ComponentMask::Material;
  }
};
template <>
struct ComponentTraits<MeshFilter> {
  static const std::string GetName() {
    return "MeshFilter";
  }
  static ComponentType GetType() {
    return ComponentType::MeshFilter;
  }
  static ComponentMask GetMask() {
    return ComponentMask::MeshFilter;
  }
};
template <>
struct ComponentTraits<Mesh> {
  static const std::string GetName() {
    return "Mesh";
  }
  static ComponentType GetType() {
    return ComponentType::Mesh;
  }
  static ComponentMask GetMask() {
    return ComponentMask::Mesh;
  }
};
template <>
struct ComponentTraits<MeshRenderer> {
  static const std::string GetName() {
    return "MeshRenderer";
  }
  static ComponentType GetType() {
    return ComponentType::MeshRenderer;
  }
  static ComponentMask GetMask() {
    return ComponentMask::MeshRenderer;
  }
};
template <>
struct ComponentTraits<Skybox> {
  static const std::string GetName() {
    return "Skybox";
  }
  static ComponentType GetType() {
    return ComponentType::Skybox;
  }
  static ComponentMask GetMask() {
    return ComponentMask::Skybox;
  }
};
template <>
struct ComponentTraits<Texture> {
  static const std::string GetName() {
    return "Texture";
  }
  static ComponentType GetType() {
    return ComponentType::Texture;
  }
  static ComponentMask GetMask() {
    return ComponentMask::Texture;
  }
};
template <>
struct ComponentTraits<Transform> {
  static const std::string GetName() {
    return "Transform";
  }
  static ComponentType GetType() {
    return ComponentType::Transform;
  }
  static ComponentMask GetMask() {
    return ComponentMask::Transform;
  }
};
} // namespace kuki
