#pragma once
#include <glad/glad.h>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <unordered_map>
#include <vector>
#include <variant>
#include <string>
template <typename T>
class ComponentManager {
private:
  std::vector<T> components;
  std::unordered_map<unsigned int, unsigned int> entityToComponent;
  std::vector<unsigned int> componentToEntity;
public:
  T& AddComponent(unsigned int);
  void RemoveComponent(unsigned int);
  bool HasComponent(unsigned int);
  T& GetComponent(unsigned int);
  T& GetDefault();
  const unsigned int GetEntityID(unsigned int) const;
  typename std::vector<T>::const_iterator begin();
  typename std::vector<T>::const_iterator end();
  void CleanUp();
};
struct Property {
  std::string name;
  std::variant<GLuint, GLsizei, glm::vec3> value;
};
struct IComponent {
  virtual ~IComponent() = default;
  virtual std::vector<Property> GetProperties() const = 0;
};
struct Transform : IComponent {
  glm::vec3 position = glm::vec3(.0f);
  glm::vec3 rotation = glm::vec3(.0f);
  glm::vec3 scale = glm::vec3(1.0f);
  std::vector<Property> GetProperties() const override {
    return {{"Position", position}, {"Rotation", rotation}, {"Scale", scale}};
  }
};
struct MeshRenderer : IComponent {
  GLuint shader = 0;
  std::vector<Property> GetProperties() const override {
    return {{"Shader ID", shader}};
  }
};
struct MeshFilter : IComponent {
  GLuint vao = 0;
  GLuint vbo = 0;
  GLuint ebo = 0;
  GLsizei vertexCount = 0;
  GLsizei indexCount = 0;
  std::vector<Property> GetProperties() const override {
    return {{"VAO", vao}, {"VBO", vbo}, {"EBO", ebo}, {"Vertex Count", vertexCount}, {"Index Count", indexCount}};
  }
};
template <typename T>
T& ComponentManager<T>::AddComponent(unsigned int entityID) {
  if (entityToComponent.find(entityID) != entityToComponent.end())
    return components[entityToComponent[entityID]];
  T component{};
  auto componentID = components.size();
  components.push_back(component);
  entityToComponent[entityID] = componentID;
  componentToEntity.push_back(entityID);
  return components[componentID];
}
template <typename T>
void ComponentManager<T>::RemoveComponent(unsigned int entityID) {
  if (entityToComponent.find(entityID) == entityToComponent.end())
    return;
  auto count = components.size();
  if (count <= 0)
    return;
  auto componentID = entityToComponent[entityID];
  auto lastID = count - 1;
  if (componentID != lastID) {
    std::swap(components[componentID], components[lastID]);
    std::swap(componentToEntity[componentID], componentToEntity[lastID]);
    entityToComponent[componentToEntity[componentID]] = componentID;
  }
  entityToComponent.erase(entityID);
  componentToEntity.pop_back();
  components.pop_back();
}
template <typename T>
bool ComponentManager<T>::HasComponent(unsigned int entityID) {
  return entityToComponent.find(entityID) != entityToComponent.end();
}
template <typename T>
T& ComponentManager<T>::GetComponent(unsigned int entityID) {
  auto it = entityToComponent.find(entityID);
  if (it == entityToComponent.end())
    return GetDefault();
  return components[it->second];
}
template <typename T>
T& ComponentManager<T>::GetDefault() {
  // FIXME: this should not be modified by the caller
  static T defaultValue{};
  return defaultValue;
}
template <typename T>
const unsigned int ComponentManager<T>::GetEntityID(unsigned int componentID) const {
  return componentToEntity[componentID];
}
template <typename T>
typename std::vector<T>::const_iterator ComponentManager<T>::begin() {
  return components.cbegin();
}
template <typename T>
typename std::vector<T>::const_iterator ComponentManager<T>::end() {
  return components.cend();
}
template <>
inline void ComponentManager<MeshRenderer>::CleanUp() {
  for (const auto& renderer : components)
    glDeleteProgram(renderer.shader);
}
template <>
inline void ComponentManager<MeshFilter>::CleanUp() {
  for (const auto& filter : components) {
    glDeleteVertexArrays(1, &filter.vao);
    glDeleteBuffers(1, &filter.vbo);
    glDeleteBuffers(1, &filter.ebo);
  }
}
