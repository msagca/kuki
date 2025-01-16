#pragma once
#include "component/mesh.hpp"
#include "component/shader.hpp"
#include "component/texture.hpp"
#include <glad/glad.h>
#include <unordered_map>
#include <utility>
#include <vector>
template <typename T>
class ComponentManager {
private:
  std::vector<T> components;
  std::unordered_map<unsigned int, unsigned int> entityToComponent;
  std::vector<unsigned int> componentToEntity;
  unsigned int inactiveCount = 0; // TODO: shrink the array if inactive count gets too high to reclaim some memory
  template <typename F>
  void ForAll(F);
public:
  unsigned int ActiveCount();
  unsigned int InactiveCount();
  T& Add(unsigned int);
  void Remove(unsigned int);
  bool Has(unsigned int);
  T* Get(unsigned int);
  T* GetFirst();
  template <typename F>
  void ForEach(F);
  void CleanUp();
};
template <typename T>
unsigned int ComponentManager<T>::ActiveCount() {
  return components.size() - inactiveCount;
}
template <typename T>
unsigned int ComponentManager<T>::InactiveCount() {
  return inactiveCount;
}
template <typename T>
T& ComponentManager<T>::Add(unsigned int id) {
  auto it = entityToComponent.find(id);
  if (it != entityToComponent.end())
    return components[it->second];
  auto componentID = components.size();
  if (inactiveCount > 0) {
    componentID = ActiveCount();
    inactiveCount--;
  } else
    components.emplace_back();
  entityToComponent.insert({id, componentID});
  componentToEntity.push_back(id);
  return components[componentID];
}
template <typename T>
void ComponentManager<T>::Remove(unsigned int id) {
  auto it = entityToComponent.find(id);
  if (it == entityToComponent.end())
    return;
  auto componentID = it->second;
  auto lastID = ActiveCount() - 1;
  if (componentID != lastID) {
    std::swap(components[componentID], components[lastID]);
    std::swap(componentToEntity[componentID], componentToEntity[lastID]);
    entityToComponent[componentToEntity[componentID]] = componentID;
  }
  entityToComponent.erase(id);
  componentToEntity.pop_back();
  inactiveCount++;
  // TODO: delete unreferenced OpenGL buffers
}
template <typename T>
bool ComponentManager<T>::Has(unsigned int id) {
  return entityToComponent.find(id) != entityToComponent.end();
}
template <typename T>
T* ComponentManager<T>::Get(unsigned int id) {
  auto it = entityToComponent.find(id);
  if (it == entityToComponent.end())
    return nullptr;
  return &components[it->second];
}
template <typename T>
T* ComponentManager<T>::GetFirst() {
  return ActiveCount() > 0 ? &components.front() : nullptr;
}
template <typename T>
template <typename F>
void ComponentManager<T>::ForEach(F func) {
  for (auto i = 0; i < ActiveCount(); i++)
    func(components[i]);
}
template <typename T>
template <typename F>
void ComponentManager<T>::ForAll(F func) {
  for (const auto& c : components)
    func(c);
}
template <>
inline void ComponentManager<Mesh>::CleanUp() {
  ForAll([](const Mesh& mesh) {
    glDeleteBuffers(1, &mesh.vertexBuffer);
    glDeleteBuffers(1, &mesh.indexBuffer);
    glDeleteVertexArrays(1, &mesh.vertexArray);
  });
}
template <>
inline void ComponentManager<Shader>::CleanUp() {
  ForAll([](const Shader& shader) {
    glDeleteProgram(shader.id);
  });
}
template <>
inline void ComponentManager<Texture>::CleanUp() {
  ForAll([](const Texture& texture) {
    glBindTexture(GL_TEXTURE_2D, texture.id);
    glDeleteTextures(1, &texture.id);
    glBindTexture(GL_TEXTURE_2D, 0);
  });
}
