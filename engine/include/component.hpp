#pragma once
#include <glm/ext/vector_float3.hpp>
#include <glm/ext/vector_float4.hpp>
#include <kuki_engine_export.h>
#include <typeindex>
#include <vector>
namespace kuki {
struct IComponent {
  std::type_index typeIndex;
protected:
  template <typename T>
  explicit IComponent(std::in_place_type_t<T>);
public:
  IComponent(const IComponent&);
  virtual ~IComponent() = default;
  template <typename T>
  T* As();
  template <typename T>
  const T* As() const;
  template <typename T>
  bool Is() const;
};
template <typename T>
IComponent::IComponent(std::in_place_type_t<T>)
  : typeIndex(std::type_index(typeid(T))) {}
template <typename T>
T* IComponent::As() {
  return typeIndex == std::type_index(typeid(T)) ? static_cast<T*>(this) : nullptr;
}
template <typename T>
const T* IComponent::As() const {
  return typeIndex == std::type_index(typeid(T)) ? static_cast<const T*>(this) : nullptr;
}
template <typename T>
bool IComponent::Is() const {
  return typeIndex == std::type_index(typeid(T));
}
enum class CameraType : uint8_t {
  Perspective,
  Orthographic
};
enum class ComputeType : uint8_t {
  BRDF_LUT,
  CubeMapEquirect,
  EquirectCubeMap,
  IrradianceMap,
  PrefilterMap
};
enum class LightType : uint8_t {
  Directional,
  Point
};
/// @brief Each material type shall correspond to a shader
enum class MaterialType : uint8_t {
  Bloom,
  Blur,
  BrightPass,
  Lit,
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
  Equirect,
  BRDF,
  Irradiance,
  Prefilter
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
  EXR = static_cast<size_t>(1) << static_cast<uint8_t>(TextureType::EXR),
  Equirect = static_cast<size_t>(1) << static_cast<uint8_t>(TextureType::Equirect),
  BRDF = static_cast<size_t>(1) << static_cast<uint8_t>(TextureType::BRDF),
  Irradiance = static_cast<size_t>(1) << static_cast<uint8_t>(TextureType::Irradiance),
  Prefilter = static_cast<size_t>(1) << static_cast<uint8_t>(TextureType::Prefilter)
};
template <typename T>
struct EnumTraits {
  static_assert(sizeof(T) == 0, "EnumTraits must be specialized for this type.");
  static const std::vector<const char*>& GetNames();
};
template <>
struct EnumTraits<ComputeType> {
  static const std::vector<const char*>& GetNames() {
    static const std::vector<const char*> names = {"BRDF_LUT", "CubeMapEquirect", "EquirectCubeMap", "IrradianceMap", "PrefilterMap"};
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
    static const std::vector<const char*> names = {"Bloom", "Blur", "BrightPass", "Lit", "Skybox", "Unlit"};
    return names;
  }
};
template <>
struct EnumTraits<TextureType> {
  static const std::vector<const char*>& GetNames() {
    static const std::vector<const char*> names = {"Albedo", "Normal", "Metalness", "Occlusion", "Roughness", "Specular", "Emissive", "CubeMap", "HDR", "EXR", "Equirect", "BRDF", "Irradiance", "Prefilter"};
    return names;
  }
};
} // namespace kuki
