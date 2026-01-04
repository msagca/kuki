#pragma once
#include <gl_shader.hpp>
#include <gl_skybox.hpp>
namespace kuki {
class KUKI_ENGINE_API GLLitShader final : public GLShader {
public:
  auto Draw(const GLMesh &) -> void override;
  auto SetCamera(const Camera &, const unsigned int) -> void override;
  auto SetLighting(const Light &) -> void override;
  auto SetLighting(std::span<const Light>) -> void override;
  auto SetMaterialFallback(const GLMesh &, std::span<const MaterialFallback>, const unsigned int) -> void override;
  auto SetSkybox(const GLSkybox * = nullptr) -> void override;
};
} // namespace kuki
