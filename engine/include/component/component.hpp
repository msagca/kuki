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
