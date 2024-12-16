#pragma once
#include <component_types.hpp>
#include <glad/glad.h>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <unordered_map>
#include <vector>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/trigonometric.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/matrix_clip_space.hpp>
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
  T* GetComponentPtr(unsigned int);
  T& GetDefault();
  T* GetDefaultPtr();
  const unsigned int GetEntityID(unsigned int) const;
  typename std::vector<T>::const_iterator Begin();
  typename std::vector<T>::const_iterator End();
  void CleanUp();
};
template <typename T>
T& ComponentManager<T>::AddComponent(unsigned int id) {
  if (entityToComponent.find(id) != entityToComponent.end())
    return components[entityToComponent[id]];
  T component{};
  auto componentID = components.size();
  components.push_back(component);
  entityToComponent[id] = componentID;
  componentToEntity.push_back(id);
  return components[componentID];
}
template <typename T>
void ComponentManager<T>::RemoveComponent(unsigned int id) {
  if (entityToComponent.find(id) == entityToComponent.end())
    return;
  auto count = components.size();
  if (count == 0)
    return;
  auto componentID = entityToComponent[id];
  auto lastID = count - 1;
  if (componentID != lastID) {
    std::swap(components[componentID], components[lastID]);
    std::swap(componentToEntity[componentID], componentToEntity[lastID]);
    entityToComponent[componentToEntity[componentID]] = componentID;
  }
  entityToComponent.erase(id);
  componentToEntity.pop_back();
  components.pop_back();
}
template <typename T>
bool ComponentManager<T>::HasComponent(unsigned int id) {
  return entityToComponent.find(id) != entityToComponent.end();
}
template <typename T>
T& ComponentManager<T>::GetComponent(unsigned int id) {
  auto it = entityToComponent.find(id);
  if (it == entityToComponent.end())
    return GetDefault();
  return components[it->second];
}
template <typename T>
T* ComponentManager<T>::GetComponentPtr(unsigned int id) {
  auto it = entityToComponent.find(id);
  if (it == entityToComponent.end())
    return GetDefaultPtr();
  return &components[it->second];
}
template <typename T>
T& ComponentManager<T>::GetDefault() {
  // FIXME: this should not be modified by the caller
  static T defaultValue{};
  return defaultValue;
}
template <typename T>
T* ComponentManager<T>::GetDefaultPtr() {
  return &GetDefault();
}
template <typename T>
const unsigned int ComponentManager<T>::GetEntityID(unsigned int componentID) const {
  return componentToEntity[componentID];
}
template <typename T>
typename std::vector<T>::const_iterator ComponentManager<T>::Begin() {
  return components.cbegin();
}
template <typename T>
typename std::vector<T>::const_iterator ComponentManager<T>::End() {
  return components.cend();
}
struct IComponent;
struct MeshRenderer;
struct MeshFilter;
template <>
inline void ComponentManager<MeshRenderer>::CleanUp() {
  for (const auto& renderer : components)
    glDeleteProgram(renderer.shader);
}
template <>
inline void ComponentManager<MeshFilter>::CleanUp() {
  for (const auto& filter : components) {
    glDeleteVertexArrays(1, &filter.vertexArray);
    glDeleteBuffers(1, &filter.vertexBuffer);
    glDeleteBuffers(1, &filter.indexBuffer);
  }
}
