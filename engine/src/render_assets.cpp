#include <algorithm>
#include <application.hpp>
#include <component.hpp>
#include <glad/glad.h>
#include <id.hpp>
#include <pool.hpp>
#include <rendering_system.hpp>
#include <skybox.hpp>
#include <texture.hpp>
#include <texture_params.hpp>
#include <unordered_map>
namespace kuki {
int RenderingSystem::RenderAssetToTexture(ID assetId, int size) {
  static constexpr auto MIN_SIZE = 16;
  auto textureSize = std::max(MIN_SIZE, size);
  auto [texture, skybox] = app.GetAssetComponents<Texture, Skybox>(assetId);
  auto it = assetToTexture.find(assetId);
  auto isPresent = it != assetToTexture.end();
  GLuint textureIdPost = 0;
  auto isCubeMap = false;
  if (isPresent)
    textureIdPost = it->second;
  else if (texture) {
    isCubeMap = texture->type == TextureType::CubeMap;
    if (!isCubeMap)
      textureIdPost = texture->id;
  }
  if (!isPresent && !texture) {
    TextureParams textureParams{textureSize, textureSize, GL_TEXTURE_2D, GL_RGBA16F};
    textureParams.target = isCubeMap ? GL_TEXTURE_CUBE_MAP : GL_TEXTURE_2D;
    auto textureIdPre = texturePool.Request(textureParams);
    textureIdPost = texturePool.Request(textureParams);
    auto framebuffer = framebufferPool.Request(textureParams);
    auto renderbuffer = renderbufferPool.Request(textureParams);
    UpdateAttachments(textureParams, framebuffer, renderbuffer, textureIdPre);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glViewport(0, 0, textureSize, textureSize);
    glClearColor(.0f, .0f, .0f, .0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    if (isCubeMap || skybox)
      DrawSkyboxAsset(assetId);
    else
      DrawAssetHierarchy(assetId);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    framebufferPool.Release(textureParams, framebuffer);
    renderbufferPool.Release(textureParams, renderbuffer);
    ApplyPostProc(textureIdPre, textureIdPost, textureParams);
    texturePool.Release(textureParams, textureIdPre);
    assetToTexture[assetId] = textureIdPost;
  }
  if (skybox)
    skybox->preview = textureIdPost;
  return textureIdPost;
}
} // namespace kuki
