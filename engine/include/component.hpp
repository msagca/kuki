#pragma once
#include <animator.hpp>
#include <bone_data.hpp>
#include <bounding_box.hpp>
#include <camera.hpp>
#include <component_type.hpp>
#include <dx_material.hpp>
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
#include <kuki_engine_export.h>
#include <light.hpp>
#include <material_handle.hpp>
#include <mesh_handle.hpp>
#include <skeleton.hpp>
#include <skybox_handle.hpp>
#include <texture_handle.hpp>
#include <typeindex>
#include <unordered_map>
#include <variant>
namespace kuki {
using ComponentVariant = std::variant<Animator *, BoneData *, BoundingBox *, Camera *, DXMaterial *, GLBuffer *, GLComputeShader *, GLLitShader *, GLMaterial *, GLMesh *, GLRenderTarget *, GLSkybox *, GLTexture *, GLUnlitShader *, IndirectLighting *, Light *, MaterialHandle *, MeshHandle *, ModelMaterialHandle *, ModelMeshHandle *, Script *, Skeleton *, SkyboxHandle *, TextureHandle *, Transform *>;
using ConstComponentVariant = std::variant<const Animator *, const BoneData *, const BoundingBox *, const Camera *, const DXMaterial *, const GLBuffer *, const GLComputeShader *, const GLLitShader *, const GLMaterial *, const GLMesh *, const GLRenderTarget *, const GLSkybox *, const GLTexture *, const GLUnlitShader *, const IndirectLighting *, const Light *, const MaterialHandle *, const MeshHandle *, const ModelMaterialHandle *, const ModelMeshHandle *, const Script *, const Skeleton *, const SkyboxHandle *, const TextureHandle *, const Transform *>;
class KUKI_ENGINE_API Component {
public:
  static auto ForEachSetType(const ComponentMask &, auto &&) -> void;
  static auto ForEachUnsetType(const ComponentMask &, auto &&) -> void;
  static auto GetBit(const std::type_index &) -> size_t;
  static auto GetType(const std::type_index &) -> ComponentType;
  static auto GetTypeIndex(const ComponentType) -> std::type_index;
  static auto GetTypeName(const ComponentType) -> std::string;
  static auto IsGL(const ComponentType) -> bool;
  /// @brief Whether a scene may hold at most one of this component, wherever it sits.
  ///
  /// These describe the scene rather than the entity carrying them: what its indirect lighting does,
  /// and in time whatever else is a property of the whole. An entity is only where such a thing is
  /// put so that it can be selected and edited like everything else, and two of them would be two
  /// answers to a question with one -- with nothing to say which the renderer should read.
  ///
  /// Enforced where components are offered rather than where they are added, so the constraint shows
  /// as an option that is not there rather than as an action that quietly fails.
  static auto IsSceneSingleton(const ComponentType) -> bool;
private:
  static const std::unordered_map<std::type_index, ComponentType> indexToType;
  static const std::unordered_map<ComponentType, std::type_index> typeToIndex;
  static const std::unordered_map<ComponentType, std::string> typeToName;
};
auto Component::ForEachSetType(const ComponentMask &mask, auto &&func) -> void {
  for (auto i = 0; i < mask.size(); ++i)
    if (mask[i]) {
      const auto type = static_cast<ComponentType>(i);
      func(type);
    }
}
auto Component::ForEachUnsetType(const ComponentMask &mask, auto &&func) -> void {
  for (auto i = 0; i < mask.size(); ++i)
    if (!mask[i]) {
      const auto type = static_cast<ComponentType>(i);
      func(type);
    }
}
} // namespace kuki
