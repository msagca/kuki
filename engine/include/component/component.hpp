#pragma once
#include <glm/ext/vector_float3.hpp>
#include <string>
#include <variant>
#include <vector>
enum class ComponentID : unsigned int {
  CameraID,
  LightID,
  MaterialID,
  MeshFilterID,
  MeshID,
  MeshRendererID,
  ShaderID,
  TextureID,
  TransformID
};
enum class ComponentMask : size_t {
  CameraMask = 1 << static_cast<unsigned int>(ComponentID::CameraID),
  LightMask = 1 << static_cast<unsigned int>(ComponentID::LightID),
  MaterialMask = 1 << static_cast<unsigned int>(ComponentID::MaterialID),
  MeshFilterMask = 1 << static_cast<unsigned int>(ComponentID::MeshFilterID),
  MeshMask = 1 << static_cast<unsigned int>(ComponentID::MeshID),
  MeshRendererMask = 1 << static_cast<unsigned int>(ComponentID::MeshRendererID),
  ShaderMask = 1 << static_cast<unsigned int>(ComponentID::ShaderID),
  TextureMask = 1 << static_cast<unsigned int>(ComponentID::TextureID),
  TransformMask = 1 << static_cast<unsigned int>(ComponentID::TransformID)
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
  ORM,
  Metalness,
  Occlusion,
  Roughness
};
struct Property {
  std::string name;
  std::variant<int, unsigned int, float, bool, glm::vec3, CameraType, LightType, TextureType> value;
};
struct IComponent {
  virtual ~IComponent() = default;
  virtual std::string GetName() const = 0;
  virtual std::vector<Property> GetProperties() const = 0;
  virtual void SetProperty(Property) = 0;
};
