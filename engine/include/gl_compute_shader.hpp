#pragma once
#include <compute_type.hpp>
#include <gl_shader_base.hpp>
#include <shader_asset.hpp>
namespace kuki {
struct KUKI_ENGINE_API GLComputeShader final : public GLShaderBase {
  GLComputeShader();
  auto Dispatch(const unsigned int, const unsigned int, const unsigned int) const -> void;
  ComputeType type;
};
} // namespace kuki
