#pragma once
#include <bone_data.hpp>
#include <camera.hpp>
#include <compute_type.hpp>
#include <gl_material.hpp>
#include <gl_mesh.hpp>
#include <gl_shader_base.hpp>
#include <gl_skybox.hpp>
#include <glm/ext/matrix_float3x3.hpp>
#include <kuki_engine_export.h>
#include <light.hpp>
#include <material_type.hpp>
#include <shader_asset.hpp>
#include <span>
namespace kuki {
struct KUKI_ENGINE_API GLShader : public GLShaderBase {
  GLShader();
  virtual ~GLShader() = default;
  MaterialType type{MaterialType::Unknown};
  auto DrawInstanced(const GLMesh &, const unsigned int) -> void;
  auto SetBoneTransforms(const BoneData &) -> void;
  auto SetMaterial(const GLMaterial &) -> void;
  auto SetMaterialFallback(const GLMesh &, const MaterialFallback &, const unsigned int) -> void;
  auto SetTransform(const GLMesh &, const glm::mat4 &, const unsigned int) -> void;
  auto SetTransform(const GLMesh &, std::span<const glm::mat4>, const unsigned int) -> void;
  virtual auto Draw(const GLMesh &) -> void;
  virtual auto SetCamera(const Camera &, const unsigned int) -> void;
  virtual auto SetLighting(const Light &) -> void;
  virtual auto SetLighting(std::span<const Light>) -> void;
  virtual auto SetMaterialFallback(const GLMesh &, std::span<const MaterialFallback>, const unsigned int) -> void;
  virtual auto SetSkybox(const GLSkybox * = nullptr) -> void;
};
} // namespace kuki
