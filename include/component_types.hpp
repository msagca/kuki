#pragma once
#include <glm/detail/type_vec3.hpp>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/vector_float3.hpp>
#include <string>
#include <variant>
#include <vector>
enum class ComponentID : unsigned int {
  TransformID,
  MeshFilterID,
  MeshRendererID,
  CameraID,
  LightID,
  MeshID,
  TextureID,
  ShaderID,
  MaterialID
};
enum class ComponentMask : size_t {
  TransformMask = 1 << static_cast<unsigned int>(ComponentID::TransformID),
  MeshFilterMask = 1 << static_cast<unsigned int>(ComponentID::MeshFilterID),
  MeshRendererMask = 1 << static_cast<unsigned int>(ComponentID::MeshRendererID),
  CameraMask = 1 << static_cast<unsigned int>(ComponentID::CameraID),
  LightMask = 1 << static_cast<unsigned int>(ComponentID::LightID),
  MeshMask = 1 << static_cast<unsigned int>(ComponentID::MeshID),
  TextureMask = 1 << static_cast<unsigned int>(ComponentID::TextureID),
  ShaderMask = 1 << static_cast<unsigned int>(ComponentID::ShaderID),
  MaterialMask = 1 << static_cast<unsigned int>(ComponentID::MaterialID)
};
enum class LightType {
  Directional,
  Point
};
enum class TextureType {
  DiffuseMap,
  SpecularMap,
  NormalMap
};
struct Property {
  std::string name;
  std::variant<int, unsigned int, float, bool, glm::vec3, LightType, TextureType> value;
};
struct IComponent {
  virtual ~IComponent() = default;
  virtual std::string GetName() const = 0;
  virtual std::vector<Property> GetProperties() const = 0;
  virtual void SetProperty(Property) = 0;
};
struct Transform : IComponent {
  glm::vec3 position{};
  glm::vec3 rotation{}; // NOTE: these should be in radians and converted to degrees when displayed in the editor
  glm::vec3 scale{1.0f};
  int parent{-1}; // parent entity/asset index
  std::string GetName() const override {
    return "Transform";
  }
  std::vector<Property> GetProperties() const override {
    return {{"Position", position}, {"Rotation", rotation}, {"Scale", scale}, {"Parent", parent}};
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
    } else if (std::holds_alternative<int>(property.value)) {
      auto& value = std::get<int>(property.value);
      if (property.name == "Parent")
        parent = value;
    }
  }
};
struct Camera : IComponent {
  // TODO: add the orthogonal projection option
  glm::mat4 view{};
  glm::mat4 projection{};
  glm::vec3 position{};
  glm::vec3 front{.0f, .0f, -1.0f};
  glm::vec3 up{.0f, 1.0f, .0f};
  glm::vec3 right{1.0f, .0f, .0f};
  float pitch{};
  float yaw{-90.0f};
  float fov{45.0f};
  float aspect{1.33f};
  float near{.1f};
  float far{100.0f};
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
      if (property.name == "Position")
        position = std::get<glm::vec3>(property.value);
  }
};
struct Light : IComponent {
  LightType type = LightType::Directional;
  glm::vec3 vector{.2f, 1.0f, .3f};
  glm::vec3 ambient{.2f};
  glm::vec3 diffuse{.5f};
  glm::vec3 specular{1.0f};
  // attenuation terms (for point light)
  float constant{1.0f};
  float linear{.09f};
  float quadratic{.032f};
  std::string GetName() const override {
    return "Light";
  }
  std::vector<Property> GetProperties() const override {
    return {{"Type", type}, {"Vector", vector}, {"Ambient", ambient}, {"Diffuse", diffuse}, {"Specular", specular}, {"Constant", constant}, {"Linear", linear}, {"Quadratic", quadratic}};
  }
  void SetProperty(Property property) override {
    if (std::holds_alternative<glm::vec3>(property.value)) {
      auto& value = std::get<glm::vec3>(property.value);
      if (property.name == "Vector")
        vector = value;
      else if (property.name == "Ambient")
        ambient = value;
      else if (property.name == "Diffuse")
        diffuse = value;
      else if (property.name == "Specular")
        specular = value;
    } else if (std::holds_alternative<float>(property.value)) {
      auto& value = std::get<float>(property.value);
      if (property.name == "Constant")
        constant = value;
      else if (property.name == "Linear")
        linear = value;
      else if (property.name == "Quadratic")
        quadratic = value;
    } else if (std::holds_alternative<LightType>(property.value))
      if (property.name == "Type")
        type = std::get<LightType>(property.value);
  }
};
struct BoundingBox : IComponent {
  glm::vec3 min{-.5f};
  glm::vec3 max{.5f};
  std::string GetName() const override {
    return "BoundingBox";
  }
  std::vector<Property> GetProperties() const override {
    return {{"Min", min}, {"Max", max}};
  }
  void SetProperty(Property property) override {
    if (std::holds_alternative<glm::vec3>(property.value)) {
      auto& value = std::get<glm::vec3>(property.value);
      if (property.name == "Min")
        min = value;
      else if (property.name == "Max")
        max = value;
    }
  }
};
struct Mesh : IComponent {
  unsigned int vertexArray{};
  unsigned int vertexBuffer{};
  unsigned int indexBuffer{};
  int vertexCount{}; // NOTE: includes duplicates if no EBO is used
  int indexCount{};
  std::string GetName() const override {
    return "Mesh";
  }
  std::vector<Property> GetProperties() const override {
    return {{"VertexArray", vertexArray}, {"VertexBuffer", vertexBuffer}, {"IndexBuffer", indexBuffer}, {"VertexCount", vertexCount}, {"IndexCount", indexCount}};
  }
  void SetProperty(Property property) override {
    if (std::holds_alternative<unsigned int>(property.value)) {
      auto& value = std::get<unsigned int>(property.value);
      if (property.name == "VertexArray")
        vertexArray = value;
      else if (property.name == "VertexBuffer")
        vertexBuffer = value;
      else if (property.name == "IndexBuffer")
        indexBuffer = value;
    } else if (std::holds_alternative<int>(property.value)) {
      auto& value = std::get<int>(property.value);
      if (property.name == "VertexCount")
        vertexCount = value;
      else if (property.name == "IndexCount")
        indexCount = value;
    }
  }
};
struct MeshFilter : IComponent {
  Mesh mesh{};
  BoundingBox box{};
  std::string GetName() const override {
    return "MeshFilter";
  }
  std::vector<Property> GetProperties() const override {
    return mesh.GetProperties();
  }
  void SetProperty(Property property) override {
    // FIXME: if multiple subcomponents share a property with the same type/ID combination, they all get updated
    mesh.SetProperty(property);
    box.SetProperty(property);
  }
};
struct Texture : IComponent {
  unsigned int id{};
  TextureType type{};
  int width{};
  int height{};
  std::string GetName() const override {
    return "Texture";
  }
  std::vector<Property> GetProperties() const override {
    return {{"ID", id}, {"Type", type}, {"Width", width}, {"Height", height}};
  }
  void SetProperty(Property property) override {
    if (std::holds_alternative<unsigned int>(property.value)) {
      auto& value = std::get<unsigned int>(property.value);
      if (property.name == "ID")
        id = value;
    } else if (std::holds_alternative<int>(property.value)) {
      auto& value = std::get<int>(property.value);
      if (property.name == "Width")
        width = value;
      else if (property.name == "Height")
        height = value;
    } else if (std::holds_alternative<TextureType>(property.value)) {
      auto& value = std::get<TextureType>(property.value);
      if (property.name == "Type")
        type = value;
    }
  }
};
struct Material : IComponent {
  glm::vec3 diffuse{.7f, .7f, .7f};
  glm::vec3 specular{.5f};
  float shininess{32.0f};
  Texture diffuseMap{};
  Texture specularMap{};
  Texture normalMap{};
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
      if (property.name == "Shininess")
        shininess = std::get<float>(property.value);
  }
};
struct Shader : IComponent {
  unsigned int id{};
  std::string GetName() const override {
    return "Shader";
  }
  std::vector<Property> GetProperties() const override {
    // TODO: the editor should display a friendly name instead of a shader ID; also, a preview of the shader would be nice
    return {{"ID", id}};
  }
  void SetProperty(Property property) override {
    if (std::holds_alternative<unsigned int>(property.value))
      if (property.name == "ID")
        id = std::get<unsigned int>(property.value);
  }
};
struct MeshRenderer : IComponent {
  Shader shader{};
  Material material{};
  std::string GetName() const override {
    return "MeshRenderer";
  }
  std::vector<Property> GetProperties() const override {
    // TODO: if some properties (from different subcomponents) share the same name, ImGui will display errors; to prevent this, we can prepend the subcomponent name to its property names
    auto properties = shader.GetProperties();
    auto materialProperties = material.GetProperties();
    properties.insert(properties.end(), materialProperties.begin(), materialProperties.end());
    return properties;
  }
  void SetProperty(Property property) override {
    shader.SetProperty(property);
    material.SetProperty(property);
  }
};
