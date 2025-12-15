#include <glad/glad.h>
template <typename... T>
requires(std::same_as<T, unsigned int> && ...)
bool RenderingSystem::UpdateAttachments(const TextureParams& params, unsigned int framebuffer, unsigned int renderbuffer, T... textures) {
  if (framebuffer == 0) {
    spdlog::error("Framebuffer is not initialized.");
    return false;
  }
  if (renderbuffer == 0) {
    spdlog::error("Renderbuffer attachment is not initialized.");
    return false;
  }
  auto CheckTexture = [](auto texture) {
    if (texture == 0) {
      spdlog::error("Texture attachment is not initialized.");
      return false;
    }
    return true;
  };
  if (!(CheckTexture(textures) && ...))
    return false;
  glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
  auto attachment = GL_COLOR_ATTACHMENT0;
  auto bindTexture = [&attachment, &params](auto texture) mutable {
    glFramebufferTexture2D(GL_FRAMEBUFFER, attachment, params.target, texture, 0);
    ++attachment;
  };
  if (params.target != GL_TEXTURE_CUBE_MAP && params.mipmaps <= 1)
    // NOTE: for cube maps and mipmapped textures, the caller is responsible for attaching the textures properly
    (bindTexture(textures), ...);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, renderbuffer);
  // FIXME: currently I'm calling this function multiple times every frame; hence, commenting out the check for performance reasons
  //auto status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  /*if (status != GL_FRAMEBUFFER_COMPLETE) {
    spdlog::error("Framebuffer is incomplete ({0:x}).", status);
    return false;
  }*/
  return true;
}
