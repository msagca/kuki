#include <bone_data.hpp>
#include <camera.hpp>
#include <component.hpp>
#include <component_type.hpp>
#include <gl_buffer.hpp>
#include <gl_compute_shader.hpp>
#include <gl_lit_shader.hpp>
#include <gl_material.hpp>
#include <gl_mesh.hpp>
#include <gl_render_target.hpp>
#include <gl_skybox.hpp>
#include <gl_texture.hpp>
#include <gl_unlit_shader.hpp>
#include <light.hpp>
#include <material_handle.hpp>
#include <mesh_handle.hpp>
#include <script.hpp>
#include <skybox_handle.hpp>
#include <typeindex>
#include <unordered_map>
namespace kuki {
const std::unordered_map<std::type_index, ComponentType> Component::indexToType = {
  {typeid(BoneData), ComponentType::BoneData},
  {typeid(BoundingBox), ComponentType::BoundingBox},
  {typeid(Camera), ComponentType::Camera},
  {typeid(GLBuffer), ComponentType::GLBuffer},
  {typeid(GLComputeShader), ComponentType::GLComputeShader},
  {typeid(GLLitShader), ComponentType::GLLitShader},
  {typeid(GLMaterial), ComponentType::GLMaterial},
  {typeid(GLMesh), ComponentType::GLMesh},
  {typeid(GLRenderTarget), ComponentType::GLRenderTarget},
  {typeid(GLSkybox), ComponentType::GLSkybox},
  {typeid(GLTexture), ComponentType::GLTexture},
  {typeid(GLUnlitShader), ComponentType::GLUnlitShader},
  {typeid(Light), ComponentType::Light},
  {typeid(MaterialHandle), ComponentType::MaterialHandle},
  {typeid(MeshHandle), ComponentType::MeshHandle},
  {typeid(SceneMaterialHandle), ComponentType::SceneMaterialHandle},
  {typeid(SceneMeshHandle), ComponentType::SceneMeshHandle},
  {typeid(Script), ComponentType::Script},
  {typeid(SkyboxHandle), ComponentType::SkyboxHandle},
  {typeid(TextureHandle), ComponentType::TextureHandle},
  {typeid(Transform), ComponentType::Transform}};
const std::unordered_map<ComponentType, std::type_index> Component::typeToIndex = {
  {ComponentType::BoneData, typeid(BoneData)},
  {ComponentType::BoundingBox, typeid(BoundingBox)},
  {ComponentType::Camera, typeid(Camera)},
  {ComponentType::GLBuffer, typeid(GLBuffer)},
  {ComponentType::GLComputeShader, typeid(GLComputeShader)},
  {ComponentType::GLLitShader, typeid(GLLitShader)},
  {ComponentType::GLMaterial, typeid(GLMaterial)},
  {ComponentType::GLMesh, typeid(GLMesh)},
  {ComponentType::GLRenderTarget, typeid(GLRenderTarget)},
  {ComponentType::GLSkybox, typeid(GLSkybox)},
  {ComponentType::GLTexture, typeid(GLTexture)},
  {ComponentType::GLUnlitShader, typeid(GLUnlitShader)},
  {ComponentType::Light, typeid(Light)},
  {ComponentType::MaterialHandle, typeid(SceneMaterialHandle)},
  {ComponentType::MeshHandle, typeid(SceneMeshHandle)},
  {ComponentType::SceneMaterialHandle, typeid(SceneMaterialHandle)},
  {ComponentType::SceneMeshHandle, typeid(SceneMeshHandle)},
  {ComponentType::Script, typeid(Script)},
  {ComponentType::SkyboxHandle, typeid(SkyboxHandle)},
  {ComponentType::TextureHandle, typeid(TextureHandle)},
  {ComponentType::Transform, typeid(Transform)}};
const std::unordered_map<ComponentType, std::string> Component::typeToName = {
  {ComponentType::BoneData, "BoneData"},
  {ComponentType::BoundingBox, "BoundingBox"},
  {ComponentType::Camera, "Camera"},
  {ComponentType::GLBuffer, "GLBuffer"},
  {ComponentType::GLComputeShader, "GLComputeShader"},
  {ComponentType::GLLitShader, "GLLitShader"},
  {ComponentType::GLMaterial, "GLMaterial"},
  {ComponentType::GLMesh, "GLMesh"},
  {ComponentType::GLRenderTarget, "GLRenderTarget"},
  {ComponentType::GLSkybox, "GLSkybox"},
  {ComponentType::GLTexture, "GLTexture"},
  {ComponentType::GLUnlitShader, "GLUnlitShader"},
  {ComponentType::Light, "Light"},
  {ComponentType::MaterialHandle, "MaterialHandle"},
  {ComponentType::MeshHandle, "MeshHandle"},
  {ComponentType::SceneMaterialHandle, "SceneMaterialHandle"},
  {ComponentType::SceneMeshHandle, "SceneMeshHandle"},
  {ComponentType::Script, "Script"},
  {ComponentType::SkyboxHandle, "SkyboxHandle"},
  {ComponentType::TextureHandle, "TextureHandle"},
  {ComponentType::Transform, "Transform"}};
auto Component::GetBit(const std::type_index &typeIndex) -> size_t {
  if (auto it = indexToType.find(typeIndex); it != indexToType.end())
    return static_cast<size_t>(it->second);
  return static_cast<size_t>(ComponentType::Transform);
}
auto Component::GetType(const std::type_index &typeIndex) -> ComponentType {
  if (auto it = indexToType.find(typeIndex); it != indexToType.end())
    return it->second;
  return ComponentType::Transform;
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
