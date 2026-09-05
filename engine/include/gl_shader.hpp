#pragma once
#include <indirect_lighting.hpp>
#include <camera.hpp>
#include <cstdint>
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
class KUKI_ENGINE_API GLShader : public GLShaderBase {
public:
  virtual ~GLShader() = default;
  MaterialType type{MaterialType::Unlit};
  auto Draw(const GLMesh &, const unsigned int = 1) -> void;
  auto SetBoneTransforms(std::span<const glm::mat4>, const unsigned int) -> void;
  auto SetEntityIds(const GLMesh &, std::span<const uint32_t>, const unsigned int) -> void;
  auto SetMaterial(const GLMaterial &) const -> void;
  auto SetMaterialFallback(const GLMesh &, const MaterialFallback &, const unsigned int) -> void;
  auto SetTransform(const GLMesh &, const glm::mat4 &, const unsigned int) -> void;
  auto SetTransform(const GLMesh &, std::span<const glm::mat4>, const unsigned int) -> void;
  /// @brief Binds the camera uniform block and uploads the camera transform into the given buffer.
  ///
  /// The caller owns the buffer and must have allocated at least `sizeof(CameraTransform)` bytes in it; no size check happens here.
  virtual auto SetCamera(const Camera &, const unsigned int) -> void;
  virtual auto SetLighting() -> void;
  virtual auto SetLighting(const Light &) -> void;
  virtual auto SetLighting(std::span<const Light>) -> void;
  virtual auto SetMaterialFallback(const GLMesh &, std::span<const MaterialFallback>, const unsigned int) -> void;
  virtual auto SetSkybox(const GLSkybox * = nullptr) -> void;
  /// @brief Hands over the indirect lighting values this backend has a use for.
  ///
  /// Two of the sixteen: the sky scale and the flat fallback. The rest shape a probe field that
  /// this backend has not got. A shader with no ambient term of its own ignores the call.
  virtual auto SetIndirectLighting(const IndirectLighting &) -> void;
protected:
  GLShader() = default;
  size_t entityIdCount{};
  size_t materialCount{};
  size_t transformCount{};
  GenCount cameraDirty{};
};
} // namespace kuki
