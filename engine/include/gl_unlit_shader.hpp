#pragma once
#include <gl_shader.hpp>
namespace kuki {
struct KUKI_ENGINE_API GLUnlitShader final : public GLShader {
  auto Draw(const GLMesh &) -> void override;
  auto SetMaterialFallback(const GLMesh &, std::span<const MaterialFallback>, const unsigned int) -> void override;
};
} // namespace kuki
