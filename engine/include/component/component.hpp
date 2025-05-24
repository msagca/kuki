#pragma once
#include <glm/ext/vector_float3.hpp>
#include <glm/ext/vector_float4.hpp>
#include <kuki_engine_export.h>
#include <string>
#include <variant>
#include <vector>
namespace kuki {
enum class ComponentType : uint8_t {
  Camera,
  Light,
  Material,
  MeshFilter,
  Mesh,
  MeshRenderer,
  Script,
  Skybox,
  Texture,
  Transform
};
enum class ComponentMask : size_t {
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
enum class CameraType : uint8_t {
  Perspective,
  Orthographic
};
enum class ComputeType : uint8_t {
  BRDF
};
enum class LightType : uint8_t {
  Directional,
  Point
};
/// @brief Each material type shall correspond to a shader
enum class MaterialType : uint8_t {
  CubeMapEquirect,
  CubeMapIrradiance,
  CubeMapPrefilter,
  EquirectCubeMap,
  Lit,
  Postprocessing,
  Skybox,
  Unlit
};
enum class TextureType : uint8_t {
  Albedo,
  Normal,
  Metalness,
  Occlusion,
  Roughness,
  Specular,
  Emissive,
  CubeMap,
  HDR,
  EXR, // same as HDR, but this requires flipping as TinyEXR uses a different coordinate system for textures
  BRDF
};
enum class TextureMask : size_t {
  Albedo = static_cast<size_t>(1) << static_cast<uint8_t>(TextureType::Albedo),
  Normal = static_cast<size_t>(1) << static_cast<uint8_t>(TextureType::Normal),
  Metalness = static_cast<size_t>(1) << static_cast<uint8_t>(TextureType::Metalness),
  Occlusion = static_cast<size_t>(1) << static_cast<uint8_t>(TextureType::Occlusion),
  Roughness = static_cast<size_t>(1) << static_cast<uint8_t>(TextureType::Roughness),
  Specular = static_cast<size_t>(1) << static_cast<uint8_t>(TextureType::Specular),
  Emissive = static_cast<size_t>(1) << static_cast<uint8_t>(TextureType::Emissive),
  CubeMap = static_cast<size_t>(1) << static_cast<uint8_t>(TextureType::CubeMap),
  HDR = static_cast<size_t>(1) << static_cast<uint8_t>(TextureType::HDR),
  EXR = static_cast<size_t>(1) << static_cast<uint8_t>(TextureType::EXR)
};
enum class PropertyType : uint8_t {
  Color, // color wheel
  Number,
  NumberRange, // slider
  Scale,
  Script, // dropdown
  Skybox, // image pair
  Texture // image
};
struct SkyboxData {
  int skybox;
  int irradiance;
  int prefilter;
  int brdf;
  int preview;
};
struct KUKI_ENGINE_API Property {
  using PropertyValue = std::variant<int, float, bool, glm::vec3, glm::vec4, CameraType, LightType, MaterialType, TextureType, SkyboxData>;
  std::string name{};
  PropertyValue value{};
  PropertyType type{PropertyType::Number};
  Property(const std::string& = "", const PropertyValue& = 0, PropertyType = PropertyType::Number);
};
struct IComponent {
  virtual ~IComponent() = default;
  virtual const std::string GetName() const = 0;
  virtual std::vector<Property> GetProperties() const = 0;
  virtual void SetProperty(Property) = 0;
};
template <typename T>
struct EnumTraits {
  static_assert(sizeof(T) == 0, "EnumTraits must be specialized for this type.");
  static const std::vector<const char*>& GetNames();
};
template <>
struct EnumTraits<ComputeType> {
  static const std::vector<const char*>& GetNames() {
    static const std::vector<const char*> names = {"BRDF"};
    return names;
  }
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
struct EnumTraits<MaterialType> {
  static const std::vector<const char*>& GetNames() {
    static const std::vector<const char*> names = {"CubeMapEquirect", "CubeMapIrradiance", "CubeMapPrefilter", "EquirectCubeMap", "Lit", "Postprocessing", "Skybox", "Unlit"};
    return names;
  }
};
template <>
struct EnumTraits<TextureType> {
  static const std::vector<const char*>& GetNames() {
    static const std::vector<const char*> names = {"Albedo", "Normal", "Metalness", "Occlusion", "Roughness", "Specular", "Emissive", "CubeMap", "HDR", "EXR", "BRDF"};
    return names;
  }
};
template <>
struct EnumTraits<PropertyType> {
  static const std::vector<const char*>& GetNames() {
    static const std::vector<const char*> names = {"Color", "Number", "NumberRange", "Script", "Texture "};
    return names;
  }
};
template <typename T>
struct ComponentTraits {
  static_assert(sizeof(T) == 0, "ComponentTraits must be specialized for this type.");
  static const std::string GetName();
  static ComponentType GetId();
  static ComponentMask GetMask();
};
struct Camera;
struct Light;
struct Material;
struct MeshFilter;
struct Mesh;
struct MeshRenderer;
struct Script;
struct Skybox;
struct Texture;
struct Transform;
template <>
struct ComponentTraits<Camera> {
  static const std::string GetName() {
    return "Camera";
  }
  static ComponentType GetId() {
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
  static ComponentType GetId() {
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
  static ComponentType GetId() {
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
  static ComponentType GetId() {
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
  static ComponentType GetId() {
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
  static ComponentType GetId() {
    return ComponentType::MeshRenderer;
  }
  static ComponentMask GetMask() {
    return ComponentMask::MeshRenderer;
  }
};
template <>
struct ComponentTraits<Script> {
  static const std::string GetName() {
    return "Script";
  }
  static ComponentType GetId() {
    return ComponentType::Script;
  }
  static ComponentMask GetMask() {
    return ComponentMask::Script;
  }
};
template <>
struct ComponentTraits<Skybox> {
  static const std::string GetName() {
    return "Skybox";
  }
  static ComponentType GetId() {
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
  static ComponentType GetId() {
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
  static ComponentType GetId() {
    return ComponentType::Transform;
  }
  static ComponentMask GetMask() {
    return ComponentMask::Transform;
  }
};
} // namespace kuki
