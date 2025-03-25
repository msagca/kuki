#pragma once
#include "component.hpp"
#include "shader.hpp"
#include <engine_export.h>
#include <typeindex>
struct IMaterial : IComponent {
  virtual ~IMaterial() = default;
  virtual void Apply(Shader& shader) const = 0;
};
struct ENGINE_API PhongMaterial final : IMaterial {
  glm::vec3 diffuse = glm::vec3(.5f, .5f, .5f);
  glm::vec3 specular = glm::vec3(1.0f);
  float shininess = .5f;
  void Apply(Shader&) const override;
  const std::string GetName() const override;
  std::vector<Property> GetProperties() const override;
  void SetProperty(Property property) override;
};
struct ENGINE_API PBRMaterial final : IMaterial {
  int base = 0;
  int normal = 0;
  int metalness = 0;
  int occlusion = 0;
  int roughness = 0;
  void Apply(Shader&) const override;
  const std::string GetName() const override;
  std::vector<Property> GetProperties() const override;
  void SetProperty(Property property) override;
};
struct ENGINE_API UnlitMaterial final : IMaterial {
  int base = 0;
  void Apply(Shader&) const override;
  const std::string GetName() const override;
  std::vector<Property> GetProperties() const override;
  void SetProperty(Property property) override;
};
struct ENGINE_API Material : IMaterial {
  std::variant<PhongMaterial, PBRMaterial, UnlitMaterial> material;
  void Apply(Shader& shader) const override;
  std::type_index GetTypeIndex() const;
  const std::string GetName() const override;
  std::vector<Property> GetProperties() const override;
  void SetProperty(Property) override;
};
