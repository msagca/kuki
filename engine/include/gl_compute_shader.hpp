#pragma once
#include <gl_shader_base.hpp>
#include <kuki_engine_export.h>
#include <shader_asset.hpp>
namespace kuki {
struct KUKI_ENGINE_API GLComputeShader final : public GLShaderBase {
  auto Dispatch(const unsigned int, const unsigned int, const unsigned int) const -> void;
};
} // namespace kuki
