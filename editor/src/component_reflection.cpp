#include <animator.hpp>
#include <bone_data.hpp>
#include <bounding_box.hpp>
#include <camera.hpp>
#include <component_reflection.hpp>
#include <gl_buffer.hpp>
#include <gl_compute_shader.hpp>
#include <gl_lit_shader.hpp>
#include <gl_material.hpp>
#include <gl_mesh.hpp>
#include <gl_render_target.hpp>
#include <gl_skybox.hpp>
#include <gl_texture.hpp>
#include <gl_unlit_shader.hpp>
#include <indirect_lighting.hpp>
#include <light.hpp>
#include <material_handle.hpp>
#include <mesh_handle.hpp>
#include <skeleton.hpp>
#include <skybox_handle.hpp>
#include <texture_handle.hpp>
#include <transform.hpp>
using namespace kuki;
auto AddComponentByType(Application &app, const EntityID id, const ComponentType type) -> void {
  switch (type) {
  case ComponentType::Animator:
    app.AddEntityComponent<Animator>(id);
    break;
  case ComponentType::BoneData:
    app.AddEntityComponent<BoneData>(id);
    break;
  case ComponentType::BoundingBox:
    app.AddEntityComponent<BoundingBox>(id);
    break;
  case ComponentType::Camera:
    app.AddEntityComponent<Camera>(id);
    break;
  case ComponentType::GLBuffer:
    app.AddEntityComponent<GLBuffer>(id);
    break;
  case ComponentType::GLComputeShader:
    app.AddEntityComponent<GLComputeShader>(id);
    break;
  case ComponentType::GLLitShader:
    app.AddEntityComponent<GLLitShader>(id);
    break;
  case ComponentType::DXMaterial:
    app.AddEntityComponent<DXMaterial>(id);
    break;
  case ComponentType::GLMaterial:
    app.AddEntityComponent<GLMaterial>(id);
    break;
  case ComponentType::GLMesh:
    app.AddEntityComponent<GLMesh>(id);
    break;
  case ComponentType::GLRenderTarget:
    app.AddEntityComponent<GLRenderTarget>(id);
    break;
  case ComponentType::GLSkybox:
    app.AddEntityComponent<GLSkybox>(id);
    break;
  case ComponentType::GLTexture:
    app.AddEntityComponent<GLTexture>(id);
    break;
  case ComponentType::GLUnlitShader:
    app.AddEntityComponent<GLUnlitShader>(id);
    break;
  case ComponentType::IndirectLighting:
    app.AddEntityComponent<IndirectLighting>(id);
    break;
  case ComponentType::Light:
    app.AddEntityComponent<Light>(id);
    break;
  case ComponentType::MaterialHandle:
    app.AddEntityComponent<MaterialHandle>(id);
    break;
  case ComponentType::MeshHandle:
    app.AddEntityComponent<MeshHandle>(id);
    break;
  case ComponentType::ModelMaterialHandle:
    app.AddEntityComponent<ModelMaterialHandle>(id);
    break;
  case ComponentType::ModelMeshHandle:
    app.AddEntityComponent<ModelMeshHandle>(id);
    break;
  case ComponentType::Script:
    break;
  case ComponentType::Skeleton:
    app.AddEntityComponent<Skeleton>(id);
    break;
  case ComponentType::SkyboxHandle:
    app.AddEntityComponent<SkyboxHandle>(id);
    break;
  case ComponentType::TextureHandle:
    app.AddEntityComponent<TextureHandle>(id);
    break;
  case ComponentType::Transform:
    app.AddEntityComponent<Transform>(id);
    break;
  }
}
auto RemoveComponentByType(Application &app, const EntityID id, const ComponentType type) -> bool {
  switch (type) {
  case ComponentType::Animator:
    return app.RemoveEntityComponent<Animator>(id);
  case ComponentType::BoneData:
    return app.RemoveEntityComponent<BoneData>(id);
  case ComponentType::BoundingBox:
    return app.RemoveEntityComponent<BoundingBox>(id);
  case ComponentType::Camera:
    return app.RemoveEntityComponent<Camera>(id);
  case ComponentType::GLBuffer:
    return app.RemoveEntityComponent<GLBuffer>(id);
  case ComponentType::DXMaterial:
    return app.RemoveEntityComponent<DXMaterial>(id);
  case ComponentType::GLMaterial:
    return app.RemoveEntityComponent<GLMaterial>(id);
  case ComponentType::GLMesh:
    return app.RemoveEntityComponent<GLMesh>(id);
  case ComponentType::GLRenderTarget:
    return app.RemoveEntityComponent<GLRenderTarget>(id);
  case ComponentType::GLSkybox:
    return app.RemoveEntityComponent<GLSkybox>(id);
  case ComponentType::GLTexture:
    return app.RemoveEntityComponent<GLTexture>(id);
  case ComponentType::IndirectLighting:
    return app.RemoveEntityComponent<IndirectLighting>(id);
  case ComponentType::Light:
    return app.RemoveEntityComponent<Light>(id);
  case ComponentType::MaterialHandle:
    return app.RemoveEntityComponent<MaterialHandle>(id);
  case ComponentType::MeshHandle:
    return app.RemoveEntityComponent<MeshHandle>(id);
  case ComponentType::ModelMaterialHandle:
    return app.RemoveEntityComponent<ModelMaterialHandle>(id);
  case ComponentType::ModelMeshHandle:
    return app.RemoveEntityComponent<ModelMeshHandle>(id);
  case ComponentType::Skeleton:
    return app.RemoveEntityComponent<Skeleton>(id);
  case ComponentType::SkyboxHandle:
    return app.RemoveEntityComponent<SkyboxHandle>(id);
  case ComponentType::TextureHandle:
    return app.RemoveEntityComponent<TextureHandle>(id);
  case ComponentType::Transform:
    return app.RemoveEntityComponent<Transform>(id);
  default:
    return false;
  }
}
