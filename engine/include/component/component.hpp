#pragma once
#include <glm/ext/vector_float3.hpp>
#include <string>
#include <variant>
#include <vector>
enum class ComponentID : unsigned int {
  Camera,
  Light,
  Material,
  MeshFilter,
  Mesh,
  MeshRenderer,
  Texture,
  Transform
};
enum class ComponentMask : size_t {
  Camera = 1 << static_cast<unsigned int>(ComponentID::Camera),
  Light = 1 << static_cast<unsigned int>(ComponentID::Light),
  Material = 1 << static_cast<unsigned int>(ComponentID::Material),
  MeshFilter = 1 << static_cast<unsigned int>(ComponentID::MeshFilter),
  Mesh = 1 << static_cast<unsigned int>(ComponentID::Mesh),
  MeshRenderer = 1 << static_cast<unsigned int>(ComponentID::MeshRenderer),
  Texture = 1 << static_cast<unsigned int>(ComponentID::Texture),
  Transform = 1 << static_cast<unsigned int>(ComponentID::Transform)
};
enum class CameraType : unsigned int {
  Perspective,
  Orthographic
};
enum class LightType : unsigned int {
  Directional,
  Point
};
enum class TextureType : unsigned int {
  Base,
  Normal,
  Metalness,
  Occlusion,
  Roughness,
  CubeMap
};
struct Property {
  std::string name;
  std::variant<int, unsigned int, float, bool, glm::vec3, CameraType, LightType, TextureType> value;
};
struct IComponent {
  virtual ~IComponent() = default;
  virtual const std::string GetName() const = 0;
  virtual std::vector<Property> GetProperties() const = 0;
  virtual void SetProperty(Property) = 0;
};
template <typename T>
struct ComponentTraits {
  static_assert(sizeof(T) == 0, "ComponentTraits must be specialized for this type");
  static const std::string GetName();
  static ComponentID GetID();
  static ComponentMask GetMask();
};
struct Camera;
struct Light;
struct Material;
struct MeshFilter;
struct Mesh;
struct MeshRenderer;
struct Texture;
struct Transform;
template <>
struct ComponentTraits<Camera> {
  static const std::string GetName() {
    return "Camera";
  }
  static ComponentID GetID() {
    return ComponentID::Camera;
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
  static ComponentID GetID() {
    return ComponentID::Light;
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
  static ComponentID GetID() {
    return ComponentID::Material;
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
  static ComponentID GetID() {
    return ComponentID::MeshFilter;
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
  static ComponentID GetID() {
    return ComponentID::Mesh;
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
  static ComponentID GetID() {
    return ComponentID::MeshRenderer;
  }
  static ComponentMask GetMask() {
    return ComponentMask::MeshRenderer;
  }
};
template <>
struct ComponentTraits<Texture> {
  static const std::string GetName() {
    return "Texture";
  }
  static ComponentID GetID() {
    return ComponentID::Texture;
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
  static ComponentID GetID() {
    return ComponentID::Transform;
  }
  static ComponentMask GetMask() {
    return ComponentMask::Transform;
  }
};
