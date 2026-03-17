#include <bone_data.hpp>
#include <camera.hpp>
#include <component.hpp>
#include <component_type.hpp>
#include <gl_material.hpp>
#include <gl_mesh.hpp>
#include <gl_skybox.hpp>
#include <gl_texture.hpp>
#include <light.hpp>
#include <material_handle.hpp>
#include <mesh_handle.hpp>
#include <skybox_handle.hpp>
#include <typeindex>
#include <unordered_map>
namespace kuki {
const std::unordered_map<std::type_index, ComponentType> Component::indexToType = {
  {typeid(BoneData), ComponentType::BoneData},
  {typeid(Camera), ComponentType::Camera},
  {typeid(GLMaterial), ComponentType::GLMaterial},
  {typeid(GLMesh), ComponentType::GLMesh},
  {typeid(GLSkybox), ComponentType::GLSkybox},
  {typeid(GLTexture), ComponentType::GLTexture},
  {typeid(Light), ComponentType::Light},
  {typeid(MaterialHandle), ComponentType::MaterialHandle},
  {typeid(MeshHandle), ComponentType::MeshHandle},
  {typeid(SkyboxHandle), ComponentType::SkyboxHandle},
  {typeid(Transform), ComponentType::Transform}};
const std::unordered_map<ComponentType, std::type_index> Component::typeToIndex = {
  {ComponentType::BoneData, typeid(BoneData)},
  {ComponentType::Camera, typeid(Camera)},
  {ComponentType::GLMaterial, typeid(GLMaterial)},
  {ComponentType::GLMesh, typeid(GLMesh)},
  {ComponentType::GLSkybox, typeid(GLSkybox)},
  {ComponentType::GLTexture, typeid(GLTexture)},
  {ComponentType::Light, typeid(Light)},
  {ComponentType::MaterialHandle, typeid(MaterialHandle)},
  {ComponentType::MeshHandle, typeid(MeshHandle)},
  {ComponentType::SkyboxHandle, typeid(SkyboxHandle)},
  {ComponentType::Transform, typeid(Transform)}};
const std::unordered_map<ComponentType, std::string> Component::typeToName = {
  {ComponentType::BoneData, "BoneData"},
  {ComponentType::Camera, "Camera"},
  {ComponentType::GLMaterial, "GLMaterial"},
  {ComponentType::GLMesh, "GLMesh"},
  {ComponentType::GLSkybox, "GLSkybox"},
  {ComponentType::GLTexture, "GLTexture"},
  {ComponentType::Light, "Light"},
  {ComponentType::MaterialHandle, "MaterialHandle"},
  {ComponentType::MeshHandle, "MeshHandle"},
  {ComponentType::SkyboxHandle, "SkyboxHandle"},
  {ComponentType::Transform, "Transform"}};
auto Component::GetBit(const std::type_index &typeIndex) -> size_t {
  if (auto it = indexToType.find(typeIndex); it != indexToType.end())
    return static_cast<size_t>(it->second);
  return static_cast<size_t>(ComponentType::Unknown);
}
auto Component::GetType(const std::type_index &typeIndex) -> ComponentType {
  if (auto it = indexToType.find(typeIndex); it != indexToType.end())
    return it->second;
  return ComponentType::Unknown;
}
auto Component::GetTypeIndex(const ComponentType type) -> std::type_index {
  if (auto it = typeToIndex.find(type); it != typeToIndex.end())
    return it->second;
  return typeid(void);
}
auto Component::GetTypeName(const ComponentType type) -> std::string {
  if (auto it = typeToName.find(type); it != typeToName.end())
    return it->second;
  return {};
}
} // namespace kuki
