#pragma once
#include <engine_export.h>
#include <glm/ext/vector_float3.hpp>
#include <glm/ext/vector_float4.hpp>
#include <string>
#include <variant>
#include <vector>
enum class ComponentId : unsigned int {
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
  Camera = 1 << static_cast<unsigned int>(ComponentId::Camera),
  Light = 1 << static_cast<unsigned int>(ComponentId::Light),
  Material = 1 << static_cast<unsigned int>(ComponentId::Material),
  MeshFilter = 1 << static_cast<unsigned int>(ComponentId::MeshFilter),
  Mesh = 1 << static_cast<unsigned int>(ComponentId::Mesh),
  MeshRenderer = 1 << static_cast<unsigned int>(ComponentId::MeshRenderer),
  Texture = 1 << static_cast<unsigned int>(ComponentId::Texture),
  Transform = 1 << static_cast<unsigned int>(ComponentId::Transform)
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
  Albedo,
  Normal,
  Metalness,
  Occlusion,
  Roughness,
  CubeMap
};
enum class PropertyType : unsigned int {
  Number,
  Color
};
struct ENGINE_API Property {
  using PropertyValue = std::variant<int, float, bool, glm::vec3, glm::vec4, CameraType, LightType, TextureType>;
  std::string name;
  PropertyType type;
  PropertyValue value;
  Property(const std::string&, PropertyValue, PropertyType = PropertyType::Number);
};
struct IComponent {
  virtual ~IComponent() = default;
  virtual const std::string GetName() const = 0;
  virtual std::vector<Property> GetProperties() const = 0;
  virtual void SetProperty(Property) = 0;
};
template <typename T>
struct EnumTraits {
  static_assert(sizeof(T) == 0, "EnumTraits must be specialized for this type");
  static const std::vector<const char*>& GetNames();
};
template <>
struct EnumTraits<CameraType> {
  static const std::vector<const char*>& GetNames() {
    static const std::vector<const char*> names = {"Perspective", "Orthographic"};
    return names;
  }
};
template <>
struct EnumTraits<LightType> {
  static const std::vector<const char*>& GetNames() {
    static const std::vector<const char*> names = {"Directional", "Point"};
    return names;
  }
};
template <>
struct EnumTraits<TextureType> {
  static const std::vector<const char*>& GetNames() {
    static const std::vector<const char*> names = {"Albedo", "Normal", "Metalness", "Occlusion", "Roughness", "CubeMap"};
    return names;
  }
};
template <>
struct EnumTraits<PropertyType> {
  static const std::vector<const char*>& GetNames() {
    static const std::vector<const char*> names = {"Number", "Color"};
    return names;
  }
};
template <typename T>
struct ComponentTraits {
  static_assert(sizeof(T) == 0, "ComponentTraits must be specialized for this type");
  static const std::string GetName();
  static ComponentId GetId();
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
  static ComponentId GetId() {
    return ComponentId::Camera;
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
  static ComponentId GetId() {
    return ComponentId::Light;
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
  static ComponentId GetId() {
    return ComponentId::Material;
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
  static ComponentId GetId() {
    return ComponentId::MeshFilter;
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
  static ComponentId GetId() {
    return ComponentId::Mesh;
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
  static ComponentId GetId() {
    return ComponentId::MeshRenderer;
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
  static ComponentId GetId() {
    return ComponentId::Texture;
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
  static ComponentId GetId() {
    return ComponentId::Transform;
  }
  static ComponentMask GetMask() {
    return ComponentMask::Transform;
  }
};
