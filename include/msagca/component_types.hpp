#pragma once
#include <glad/glad.h>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <string>
#include <variant>
#include <vector>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/trigonometric.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/matrix_clip_space.hpp>
struct Property {
  std::string name;
  std::variant<int, unsigned int, float, bool, glm::vec3> value;
};
struct IComponent {
  virtual ~IComponent() = default;
  virtual std::string GetName() const = 0;
  virtual std::vector<Property> GetProperties() const = 0;
  virtual void SetProperty(Property) = 0;
};
struct Transform : IComponent {
  glm::vec3 position = glm::vec3(.0f);
  glm::vec3 rotation = glm::vec3(.0f); // NOTE: these should be in radians and converted to degrees when displayed in the editor
  glm::vec3 scale = glm::vec3(1.0f);
  Transform* parent = nullptr;
  std::string GetName() const override {
    return "Transform";
  }
  std::vector<Property> GetProperties() const override {
    return {{"Position", position}, {"Rotation", rotation}, {"Scale", scale}};
  }
  void SetProperty(Property property) override {
    if (std::holds_alternative<glm::vec3>(property.value)) {
      auto& value = std::get<glm::vec3>(property.value);
      if (property.name == "Position")
        position = value;
      else if (property.name == "Rotation")
        rotation = value;
      else if (property.name == "Scale")
        scale = value;
    }
  }
};
struct Camera : IComponent {
  // TODO: add the orthogonal projection option
  glm::mat4 view = glm::mat4(1.0f);
  glm::mat4 projection = glm::mat4(1.0f);
  glm::vec3 position = glm::vec3(.0f);
  glm::vec3 front = glm::vec3(.0f, .0f, -1.0f);
  glm::vec3 up = glm::vec3(.0f, 1.0f, .0f);
  glm::vec3 right = glm::vec3(1.0f, .0f, .0f);
  float pitch = .0f;
  float yaw = -90.0f;
  float fov = 45.0f;
  float aspect = 1.33f;
  float near = .1f;
  float far = 100.0f;
  std::string GetName() const override {
    return "Camera";
  }
  std::vector<Property> GetProperties() const override {
    return {{"Position", position}, {"Pitch", pitch}, {"Yaw", yaw}, {"FOV", fov}, {"Aspect", aspect}, {"Near", near}, {"Far", far}};
  }
  void SetProperty(Property property) override {
    if (std::holds_alternative<float>(property.value)) {
      auto& value = std::get<float>(property.value);
      if (property.name == "Pitch")
        pitch = value;
      else if (property.name == "Yaw")
        yaw = value;
      else if (property.name == "FOV")
        fov = value;
      else if (property.name == "Aspect")
        aspect = value;
      else if (property.name == "Near")
        near = value;
      else if (property.name == "Far")
        far = value;
    } else if (std::holds_alternative<glm::vec3>(property.value))
      position = std::get<glm::vec3>(property.value);
  }
};
struct Light : IComponent {
  // TODO: allow different types of lights than directional
  glm::vec3 direction = glm::vec3(.3f, .4f, .0f);
  glm::vec3 ambient = glm::vec3(.2f);
  glm::vec3 diffuse = glm::vec3(.5f);
  glm::vec3 specular = glm::vec3(1.0f);
  std::string GetName() const override {
    return "Light";
  }
  std::vector<Property> GetProperties() const override {
    return {{"Direction", direction}, {"Ambient", ambient}, {"Diffuse", diffuse}, {"Specular", specular}};
  }
  void SetProperty(Property property) override {
    if (std::holds_alternative<glm::vec3>(property.value)) {
      auto& value = std::get<glm::vec3>(property.value);
      if (property.name == "Direction")
        direction = value;
      else if (property.name == "Ambient")
        ambient = value;
      else if (property.name == "Diffuse")
        diffuse = value;
      else if (property.name == "Specular")
        specular = value;
    }
  }
};
struct MeshFilter : IComponent {
  unsigned int vertexArray = 0;
  unsigned int vertexBuffer = 0;
  unsigned int normalBuffer = 0;
  unsigned int indexBuffer = 0;
  int vertexCount = 0;
  int indexCount = 0;
  std::string GetName() const override {
    return "MeshFilter";
  }
  std::vector<Property> GetProperties() const override {
    // TODO: the buffer IDs don't mean anything to the user, the editor should display a preview of the mesh instead
    return {{"VAO", vertexArray}, {"VertexBuffer", vertexBuffer}, {"NormalBuffer", normalBuffer}, {"IndexBuffer", indexBuffer}, {"VertexCount", vertexCount}, {"IndexCount", indexCount}};
  }
  void SetProperty(Property property) override {
    if (std::holds_alternative<unsigned int>(property.value)) {
      auto& value = std::get<unsigned int>(property.value);
      if (property.name == "VAO")
        vertexArray = value;
      else if (property.name == "VertexBuffer")
        vertexBuffer = value;
      else if (property.name == "NormalBuffer")
        normalBuffer = value;
      else if (property.name == "IndexBuffer")
        indexBuffer = value;
    } else if (std::holds_alternative<int>(property.value)) {
      if (property.name == "VertexCount")
        vertexCount = std::get<int>(property.value);
      else if (property.name == "IndexCount")
        indexCount = std::get<int>(property.value);
    }
  }
};
struct Material : IComponent {
  glm::vec3 diffuse = glm::vec3(1.0f, .5f, .3f);
  glm::vec3 specular = glm::vec3(.5f);
  float shininess = 32.0f;
  std::string GetName() const override {
    return "Material";
  }
  std::vector<Property> GetProperties() const override {
    return {{"Diffuse", diffuse}, {"Specular", specular}, {"Shininess", shininess}};
  }
  void SetProperty(Property property) override {
    if (std::holds_alternative<glm::vec3>(property.value)) {
      auto& value = std::get<glm::vec3>(property.value);
      if (property.name == "Diffuse")
        diffuse = value;
      else if (property.name == "Specular")
        specular = value;
    } else if (std::holds_alternative<float>(property.value))
      shininess = std::get<float>(property.value);
  }
};
struct MeshRenderer : IComponent {
  unsigned int shader = 0;
  Material material;
  std::string GetName() const override {
    return "MeshRenderer";
  }
  std::vector<Property> GetProperties() const override {
    // TODO: the editor should display a friendly name instead of a shader ID; also, a preview of the shader would be nice
    std::vector<Property> properties = {{"ShaderID", shader}};
    auto materialProperties = material.GetProperties();
    properties.insert(properties.end(), materialProperties.begin(), materialProperties.end());
    return properties;
  }
  void SetProperty(Property property) override {
    if (property.name == "ShaderID" && std::holds_alternative<unsigned int>(property.value))
      shader = std::get<unsigned int>(property.value);
    material.SetProperty(property);
  }
};
struct Primitive : IComponent {
  Transform transform;
  MeshFilter filter;
  MeshRenderer renderer;
  std::string GetName() const override {
    return "Primitive";
  }
  std::vector<Property> GetProperties() const override {
    auto properties = transform.GetProperties();
    auto filterProperties = filter.GetProperties();
    auto rendererProperties = renderer.GetProperties();
    properties.insert(properties.end(), filterProperties.begin(), filterProperties.end());
    properties.insert(properties.end(), rendererProperties.begin(), rendererProperties.end());
    return properties;
  }
  void SetProperty(Property property) override {
    // FIXME: if the following components share a property with the same name, all those values will be modified
    transform.SetProperty(property);
    filter.SetProperty(property);
    renderer.SetProperty(property);
  }
};
