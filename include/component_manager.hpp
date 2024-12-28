#pragma once
#include <component_types.hpp>
#include <glad/glad.h>
#include <unordered_map>
#include <vector>
template <typename T>
class ComponentManager {
private:
  std::vector<T> components;
  std::unordered_map<unsigned int, unsigned int> entityToComponent;
  std::vector<unsigned int> componentToEntity;
public:
  T& Add(unsigned int);
  void Remove(unsigned int);
  bool Has(unsigned int);
  T& Get(unsigned int);
  T* GetPtr(unsigned int);
  T& GetDefault();
  T* GetDefaultPtr();
  T* GetFirst();
  template <typename F>
  void ForEach(F);
  void CleanUp();
};
template <typename T>
T& ComponentManager<T>::Add(unsigned int id) {
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
void ComponentManager<T>::Remove(unsigned int id) {
  auto it = entityToComponent.find(id);
  if (it == entityToComponent.end())
    return;
  auto componentID = it->second;
  auto lastID = components.size() - 1;
  if (componentID != lastID) {
    std::swap(components[componentID], components[lastID]);
    std::swap(componentToEntity[componentID], componentToEntity[lastID]);
    entityToComponent[componentToEntity[componentID]] = componentID;
  }
  entityToComponent.erase(id);
  componentToEntity.pop_back();
  components.pop_back();
  // TODO: for some component types, relevant OpenGL buffers should be deleted (if no other component references them)
}
template <typename T>
bool ComponentManager<T>::Has(unsigned int id) {
  return entityToComponent.find(id) != entityToComponent.end();
}
template <typename T>
T& ComponentManager<T>::Get(unsigned int id) {
  auto it = entityToComponent.find(id);
  if (it == entityToComponent.end())
    return GetDefault();
  return components[it->second];
}
template <typename T>
T* ComponentManager<T>::GetPtr(unsigned int id) {
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
T* ComponentManager<T>::GetFirst() {
  return components.empty() ? nullptr : &components.front();
}
template <typename T>
template <typename F>
void ComponentManager<T>::ForEach(F func) {
  for (const auto& c : components)
    func(c);
}
template <>
inline void ComponentManager<Mesh>::CleanUp() {
  ForEach([](const Mesh& mesh) {
    glDeleteBuffers(1, &mesh.vertexBuffer);
    glDeleteBuffers(1, &mesh.indexBuffer);
    glDeleteVertexArrays(1, &mesh.vertexArray);
  });
}
template <>
inline void ComponentManager<Texture>::CleanUp() {
  ForEach([](const Texture& texture) {
    glBindTexture(GL_TEXTURE_2D, texture.id);
    glDeleteTextures(1, &texture.id);
    glBindTexture(GL_TEXTURE_2D, 0);
  });
}
template <>
inline void ComponentManager<Shader>::CleanUp() {
  ForEach([](const Shader& shader) {
    glDeleteProgram(shader.id);
  });
}
