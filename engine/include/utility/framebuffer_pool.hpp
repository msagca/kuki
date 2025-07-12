#pragma once
#include <glad/glad.h>
#include "texture_pool.hpp"
#include <kuki_engine_export.h>
namespace kuki {
class KUKI_ENGINE_API FramebufferPool final : public Pool<TextureParams, GLuint> {
private:
  GLenum GetBaseFormat(GLenum);
protected:
  GLuint Allocate(const TextureParams&) override;
public:
  ~FramebufferPool() override;
  void Clear() override;
};
} // namespace kuki
