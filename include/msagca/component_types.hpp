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
      if (property.name == "Position")
        position = std::get<glm::vec3>(property.value);
      else if (property.name == "Rotation")
        rotation = std::get<glm::vec3>(property.value);
      else if (property.name == "Scale")
        scale = std::get<glm::vec3>(property.value);
    }
  }
};
struct Camera : IComponent {
  glm::mat4 view;
  glm::mat4 projection;
  glm::vec3 position;
  glm::vec3 front = glm::vec3(.0f, .0f, -1.0f);
  glm::vec3 up = glm::vec3(.0f, 1.0f, .0f);
  glm::vec3 right;
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
      if (property.name == "Pitch")
        pitch = std::get<float>(property.value);
      else if (property.name == "Yaw")
        yaw = std::get<float>(property.value);
      else if (property.name == "FOV")
        fov = std::get<float>(property.value);
      else if (property.name == "Aspect")
        aspect = std::get<float>(property.value);
      else if (property.name == "Near")
        near = std::get<float>(property.value);
      else if (property.name == "Far")
        far = std::get<float>(property.value);
    } else if (std::holds_alternative<glm::vec3>(property.value))
      position = std::get<glm::vec3>(property.value);
  }
};
struct MeshFilter : IComponent {
  unsigned int vao = 0;
  unsigned int vbo = 0;
  unsigned int ebo = 0;
  int vertexCount = 0;
  int indexCount = 0;
  std::string GetName() const override {
    return "MeshFilter";
  }
  std::vector<Property> GetProperties() const override {
    // TODO: the buffer IDs don't mean anything to the user, the editor should display a preview of the mesh instead
    return {{"VAO", vao}, {"VBO", vbo}, {"EBO", ebo}, {"VertexCount", vertexCount}, {"IndexCount", indexCount}};
  }
  void SetProperty(Property property) override {
    if (std::holds_alternative<unsigned int>(property.value)) {
      if (property.name == "VAO")
        vao = std::get<unsigned int>(property.value);
      else if (property.name == "VBO")
        vbo = std::get<unsigned int>(property.value);
      else if (property.name == "EBO")
        ebo = std::get<unsigned int>(property.value);
    } else if (std::holds_alternative<int>(property.value)) {
      if (property.name == "VertexCount")
        vertexCount = std::get<int>(property.value);
      else if (property.name == "IndexCount")
        indexCount = std::get<int>(property.value);
    }
  }
};
struct MeshRenderer : IComponent {
  unsigned int shader = 0;
  std::string GetName() const override {
    return "MeshRenderer";
  }
  std::vector<Property> GetProperties() const override {
    // TODO: the editor should display a friendly name instead of an ID; also, a preview of the shader would be nice
    return {{"ShaderID", shader}};
  }
  void SetProperty(Property property) override {
    if (property.name == "ShaderID" && std::holds_alternative<unsigned int>(property.value))
      shader = std::get<unsigned int>(property.value);
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
    transform.SetProperty(property);
    filter.SetProperty(property);
    renderer.SetProperty(property);
  }
};
