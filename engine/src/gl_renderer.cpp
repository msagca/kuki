#include <algorithm>
#include <application.hpp>
#include <asset.hpp>
#include <asset_manager.hpp>
#include <asset_type.hpp>
#include <bounding_box.hpp>
#include <camera.hpp>
#include <camera_type.hpp>
#include <cmath>
#include <color.hpp>
#include <cstddef>
#include <enum_traits.hpp>
#include <gl_context.hpp>
#include <gl_buffer.hpp>
#include <gl_compute_shader.hpp>
#include <gl_lit_shader.hpp>
#include <gl_material.hpp>
#include <gl_mesh.hpp>
#include <gl_mesh_material.hpp>
#include <gl_render_target.hpp>
#include <gl_renderer.hpp>
#include <gl_shader.hpp>
#include <gl_skybox.hpp>
#include <gl_texture.hpp>
#include <gl_unlit_shader.hpp>
#include <glad/glad.h>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/vector_float2.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/trigonometric.hpp>
#include <id.hpp>
#include <light.hpp>
#include <light_type.hpp>
#include <limits>
#include <material_asset.hpp>
#include <material_fallback.hpp>
#include <material_handle.hpp>
#include <material_type.hpp>
#include <mesh.hpp>
#include <mesh_asset.hpp>
#include <mesh_handle.hpp>
#include <model_asset.hpp>
#include <post_process.hpp>
#include <primitive.hpp>
#include <profiler.hpp>
#include <render_target.hpp>
#include <renderer.hpp>
#include <scene.hpp>
#include <shader_asset.hpp>
#include <skeleton.hpp>
#include <skybox_handle.hpp>
#include <span>
#include <spdlog/spdlog.h>
#include <string>
#include <target_description.hpp>
#include <texture.hpp>
#include <texture_asset.hpp>
#include <texture_content.hpp>
#include <transform.hpp>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>
namespace kuki {
/// @brief S3TC enumerants, which glad does not generate because they arrive through an extension.
///
/// Their values are fixed by `EXT_texture_compression_s3tc` and `EXT_texture_sRGB`, and every
/// desktop driver that can offer the 4.5 core profile this renderer already requires offers them.
#ifndef GL_COMPRESSED_RGB_S3TC_DXT1_EXT
#define GL_COMPRESSED_RGB_S3TC_DXT1_EXT 0x83F0
#endif
#ifndef GL_COMPRESSED_RGBA_S3TC_DXT5_EXT
#define GL_COMPRESSED_RGBA_S3TC_DXT5_EXT 0x83F3
#endif
#ifndef GL_COMPRESSED_SRGB_S3TC_DXT1_EXT
#define GL_COMPRESSED_SRGB_S3TC_DXT1_EXT 0x8C4C
#endif
#ifndef GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT
#define GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT 0x8C4F
#endif
GLRenderer::GLRenderer(Application &app)
  : Renderer(std::in_place_type<GLRenderer>, app) {}
auto GLRenderer::BypassPass(const RenderPass pass, std::span<std::string> inputs, std::span<std::string> outputs) -> void {
  // See the Direct3D side: the count belongs to the pass being stood down, and the shading reads it.
  if (pass == RenderPass::SpotShadowMap)
    spotShadowCount = 0;
  Renderer::BypassPass(pass, inputs, outputs);
}
auto GLRenderer::BypassCopy(std::span<std::string> inputs, std::span<std::string> outputs) -> void {
  // The first resolved colour input, falling back to a multisampled one only if that is all there
  // is -- the reasoning is argued on the Direct3D side, and the order it depends on is the order
  // the passes were declared in, which both backends are handed alike.
  GLRenderTarget *source{};
  for (const auto &name : inputs) {
    auto target = GetTarget(name);
    if (!target || target->desc.format == TargetFormat::DEPTH)
      continue;
    if (target->desc.samples <= 1) {
      source = target;
      break;
    }
    if (!source)
      source = target;
  }
  if (outputs.empty())
    return;
  auto out = GetTarget(outputs[0]);
  if (!out)
    return;
  if (!source) {
    BypassClear(outputs);
    return;
  }
  glBindFramebuffer(GL_READ_FRAMEBUFFER, source->framebuffer);
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, out->framebuffer);
  glBlitFramebuffer(0, 0, out->desc.width, out->desc.height, 0, 0, out->desc.width, out->desc.height, GL_COLOR_BUFFER_BIT, GL_LINEAR);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
auto GLRenderer::BypassClear(std::span<std::string> outputs) -> void {
  for (const auto &name : outputs) {
    auto target = GetTarget(name);
    if (!target)
      continue;
    glBindFramebuffer(GL_FRAMEBUFFER, target->framebuffer);
    glClearColor(.0f, .0f, .0f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  }
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
auto GLRenderer::GetPoolUsage() const -> PoolUsage {
  PoolUsage usage{};
  usage += bufferPool.GetUsage();
  usage += framebufferPool.GetUsage();
  usage += renderbufferPool.GetUsage();
  usage += texturePool.GetUsage();
  return usage;
}
auto GLRenderer::ApplyAntiAliasing(std::span<std::string> inputs, std::span<std::string> outputs) -> void {
  const auto in = GetTarget(inputs, "SceneMulti");
  const auto out = GetTarget(outputs, "Scene");
  if (!in || !out)
    return;
  glBindFramebuffer(GL_READ_FRAMEBUFFER, in->framebuffer);
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, out->framebuffer);
  glBlitFramebuffer(0, 0, out->desc.width, out->desc.height, 0, 0, out->desc.width, out->desc.height, GL_COLOR_BUFFER_BIT, GL_LINEAR);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
auto GLRenderer::ApplyBloomEffect(std::span<std::string> inputs, std::span<std::string> outputs) -> void {
  if (inputs.size() != 2 || outputs.size() != 1)
    return;
  auto bloomShader = GetShader("Bloom");
  if (!bloomShader)
    return;
  const auto mesh = GetPrimitive("Frame");
  if (!mesh)
    return;
  const auto in0 = GetTarget(inputs[0]);
  const auto in1 = GetTarget(inputs[1]);
  const auto out = GetTarget(outputs[0]);
  if (!in0 || !in1 || !out)
    return;
  glBindFramebuffer(GL_FRAMEBUFFER, out->framebuffer);
  glViewport(0, 0, out->desc.width, out->desc.height);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  bloomShader->Use();
  bloomShader->SetTexture("u_image", in0->texture);
  bloomShader->SetTexture("u_imageBright", in1->texture);
  bloomShader->SetUniform("u_intensity", BLOOM_INTENSITY);
  bloomShader->SetUniform("u_model", glm::mat4(1.f));
  bloomShader->Draw(*mesh);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
auto GLRenderer::ApplyBlurEffect(std::span<std::string> inputs, std::span<std::string> outputs) -> void {
  constexpr auto NUM_PASSES = BLUR_PASS_COUNT;
  if (inputs.size() != 1 || outputs.empty())
    return;
  auto blurShader = GetShader("Blur");
  if (!blurShader)
    return;
  const auto mesh = GetPrimitive("Frame");
  if (!mesh)
    return;
  const auto in = GetTarget(inputs[0]);
  const auto out = GetTarget(outputs[0]);
  const auto ping = GetTarget(outputs[0] + "Ping");
  const auto pong = GetTarget(outputs[0] + "Pong");
  if (!in || !out || !ping || !pong)
    return;
  glBindFramebuffer(GL_FRAMEBUFFER, out->framebuffer);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glBindFramebuffer(GL_FRAMEBUFFER, ping->framebuffer);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glBindFramebuffer(GL_FRAMEBUFFER, pong->framebuffer);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glViewport(0, 0, out->desc.width, out->desc.height);
  blurShader->Use();
  blurShader->SetUniform("u_model", glm::mat4(1.f));
  for (auto i = 0u; i < NUM_PASSES; ++i) {
    const auto even = i % 2 == 0;
    const auto &srcTexture = i == 0 ? in->texture : even ? pong->texture
                                                         : ping->texture;
    const auto &dstFramebuffer = i == NUM_PASSES - 1 ? out->framebuffer : even ? ping->framebuffer
                                                                               : pong->framebuffer;
    glBindFramebuffer(GL_FRAMEBUFFER, dstFramebuffer);
    blurShader->SetTexture("u_image", srcTexture);
    blurShader->SetUniform("u_horizontal", even);
    blurShader->Draw(*mesh);
  }
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
auto GLRenderer::ApplyBrightPassFilter(std::span<std::string> inputs, std::span<std::string> outputs) -> void {
  if (inputs.size() != 1 || outputs.size() != 1)
    return;
  auto brightShader = GetShader("BrightPass");
  if (!brightShader)
    return;
  const auto mesh = GetPrimitive("Frame");
  if (!mesh)
    return;
  const auto in = GetTarget(inputs[0]);
  const auto out = GetTarget(outputs[0]);
  if (!in || !out)
    return;
  glBindFramebuffer(GL_FRAMEBUFFER, out->framebuffer);
  glViewport(0, 0, out->desc.width, out->desc.height);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  brightShader->Use();
  brightShader->SetTexture("u_image", in->texture);
  brightShader->SetUniform("u_exposure", exposure);
  brightShader->SetUniform("u_threshold", BRIGHT_PASS_THRESHOLD);
  brightShader->SetUniform("u_model", glm::mat4(1.f));
  brightShader->Draw(*mesh);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
auto GLRenderer::ApplyToneMapping(std::span<std::string> inputs, std::span<std::string> outputs) -> void {
  auto toneShader = GetShader("ToneMapping");
  if (!toneShader)
    return;
  const auto mesh = GetPrimitive("Frame");
  if (!mesh)
    return;
  if (inputs.empty() || outputs.empty())
    return;
  const auto in = GetTarget(inputs[0]);
  const auto out = GetTarget(outputs[0]);
  if (!in || !out)
    return;
  glBindFramebuffer(GL_FRAMEBUFFER, out->framebuffer);
  glViewport(0, 0, out->desc.width, out->desc.height);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  toneShader->Use();
  toneShader->SetTexture("u_image", in->texture);
  toneShader->SetUniform("u_exposure", exposure);
  toneShader->SetUniform("u_gamma", GAMMA);
  toneShader->SetUniform("u_toneMapper", static_cast<unsigned int>(toneMapper));
  toneShader->SetUniform("u_model", glm::mat4(1.f));
  toneShader->Draw(*mesh);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
auto GLRenderer::EnsureOverlayBuffer(const std::vector<Vertex> &vertices) -> void {
  if (!overlayBuffer.vao)
    CreateVertexBuffer(overlayBuffer, vertices);
  else
    glNamedBufferData(overlayBuffer.vbo, vertices.size() * sizeof(Vertex), vertices.data(), GL_DYNAMIC_DRAW);
  overlayBuffer.vertexCount = static_cast<int>(vertices.size());
}
auto GLRenderer::ApplyOverlay(std::span<std::string> inputs, std::span<std::string> outputs) -> void {
  // The picture reaches the output first and whatever happens next. Every early return below is a
  // frame with no text on it rather than a frame with nothing on it.
  BypassCopy(inputs, outputs);
  auto &overlay = app.GetOverlay();
  if (overlay.IsEmpty() || outputs.empty())
    return;
  const auto out = GetTarget(outputs[0]);
  auto overlayShader = GetShader("Overlay");
  if (!out || !overlayShader)
    return;
  const auto atlasId = overlay.GetAtlasAssetId();
  LoadAsset(atlasId);
  const auto atlas = resourceRegistry.GetComponent<GLTexture>(GetAssetResourceId(atlasId));
  if (!atlas)
    return;
  overlay.Build(out->desc.width, out->desc.height, overlayMesh, overlayRuns);
  if (overlayMesh.vertices.empty())
    return;
  EnsureOverlayBuffer(overlayMesh.vertices);
  glBindFramebuffer(GL_FRAMEBUFFER, out->framebuffer);
  glViewport(0, 0, out->desc.width, out->desc.height);
  // No depth, because there is nothing here to be behind: the text is drawn last, over a picture
  // that is already finished, and the only thing it can overlap is itself.
  glDisable(GL_DEPTH_TEST);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  overlayShader->Use();
  overlayShader->SetTexture("u_atlas", atlas->id);
  // The overlay lays its quads out in the target's pixels with y up from the bottom, which this
  // maps straight onto clip space. Both backends are handed the same mesh and build the same
  // matrix; nothing about the projection is OpenGL's rather than Direct3D's.
  overlayShader->SetUniform("u_projection", glm::ortho(.0f, static_cast<float>(out->desc.width), .0f, static_cast<float>(out->desc.height)));
  glBindVertexArray(overlayBuffer.vao);
  // One draw a run, because colour is the only thing that varies and a vertex has nowhere to put
  // it. See `OverlayRun`.
  for (const auto &run : overlayRuns) {
    overlayShader->SetUniform("u_color", run.color);
    glDrawArrays(GL_TRIANGLES, static_cast<GLint>(run.first), static_cast<GLsizei>(run.count));
  }
  glBindVertexArray(0);
  glDisable(GL_BLEND);
  glEnable(GL_DEPTH_TEST);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
auto GLRenderer::ApplyOutline(std::span<std::string> inputs, std::span<std::string> outputs) -> void {
  static constexpr size_t MAX_SELECTED = 16;
  static constexpr auto OUTLINE_THICKNESS = 2.f;
  static const auto OUTLINE_COLOR = glm::vec3(1.f, .6f, 0.f);
  auto outlineShader = GetShader("Outline");
  if (!outlineShader)
    return;
  const auto mesh = GetPrimitive("Frame");
  if (!mesh)
    return;
  if (outputs.empty())
    return;
  GLRenderTarget *sceneMulti{};
  GLRenderTarget *in{};
  for (const auto &name : inputs) {
    const auto target = GetTarget(name);
    if (!target)
      continue;
    if (!sceneMulti && target->idTexture != 0)
      sceneMulti = target;
    if (!in && target->desc.samples <= 1)
      in = target;
  }
  const auto out = GetTarget(outputs[0]);
  if (!sceneMulti || !in || !out)
    return;
  const auto width = sceneMulti->desc.width;
  const auto height = sceneMulti->desc.height;
  if (outlineIdWidth != width || outlineIdHeight != height) {
    if (outlineIdFramebuffer)
      glDeleteFramebuffers(1, &outlineIdFramebuffer);
    if (outlineIdTexture)
      glDeleteTextures(1, &outlineIdTexture);
    glCreateFramebuffers(1, &outlineIdFramebuffer);
    glCreateTextures(GL_TEXTURE_2D, 1, &outlineIdTexture);
    glTextureStorage2D(outlineIdTexture, 1, GL_RGBA8, width, height);
    glNamedFramebufferTexture(outlineIdFramebuffer, GL_COLOR_ATTACHMENT0, outlineIdTexture, 0);
    outlineIdWidth = width;
    outlineIdHeight = height;
  }
  glBindFramebuffer(GL_READ_FRAMEBUFFER, sceneMulti->framebuffer);
  glReadBuffer(GL_COLOR_ATTACHMENT1);
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, outlineIdFramebuffer);
  glDrawBuffer(GL_COLOR_ATTACHMENT0);
  glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
  glReadBuffer(GL_COLOR_ATTACHMENT0);
  glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
  const auto selected = app.GetSelectedEntities();
  glBindFramebuffer(GL_FRAMEBUFFER, out->framebuffer);
  glViewport(0, 0, out->desc.width, out->desc.height);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  outlineShader->Use();
  outlineShader->SetTexture("u_image", in->texture);
  outlineShader->SetTexture("u_idImage", outlineIdTexture);
  outlineShader->SetUniform("u_texelSize", glm::vec2(1.f / width, 1.f / height));
  outlineShader->SetUniform("u_outlineThickness", OUTLINE_THICKNESS);
  outlineShader->SetUniform("u_outlineColor", OUTLINE_COLOR);
  outlineShader->SetUniform("u_model", glm::mat4(1.f));
  const auto count = std::min(selected.size(), MAX_SELECTED);
  outlineShader->SetUniform("u_selectedCount", static_cast<unsigned int>(count));
  if (count > 0) {
    std::vector<unsigned int> selectedIds;
    selectedIds.reserve(count);
    for (size_t i = 0; i < count; ++i)
      selectedIds.push_back(static_cast<unsigned int>(static_cast<long long>(selected[i])));
    outlineShader->SetUniform("u_selectedIds[0]", static_cast<int>(count), selectedIds.data());
  }
  outlineShader->Draw(*mesh);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
auto GLRenderer::CreateShadowMap(std::span<std::string> inputs, std::span<std::string> outputs) -> void {
  auto scene = GetScene();
  if (!scene)
    return;
  const auto out = GetTarget(outputs, "ShadowMap");
  if (!out)
    return;
  glEnable(GL_DEPTH_TEST);
  glBindFramebuffer(GL_FRAMEBUFFER, out->framebuffer);
  glClear(GL_DEPTH_BUFFER_BIT);
  glViewport(0, 0, out->desc.width, out->desc.height);
  DrawMeshes();
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
auto GLRenderer::RenderScene(std::span<std::string> inputs, std::span<std::string> outputs) -> void {
  auto scene = GetScene();
  if (!scene)
    return;
  const auto out = GetTarget(outputs, "SceneMulti");
  if (!out)
    return;
  glEnable(GL_DEPTH_TEST);
  glBindFramebuffer(GL_FRAMEBUFFER, out->framebuffer);
  if (out->idTexture != 0) {
    constexpr float sceneClearColor[4] = {0.f, 0.f, 0.f, 0.f};
    constexpr float idClearColor[4] = {1.f, 1.f, 1.f, 1.f};
    glClearBufferfv(GL_COLOR, 0, sceneClearColor);
    glClearBufferfv(GL_COLOR, 1, idClearColor);
    glClear(GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
  } else
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
  glViewport(0, 0, out->desc.width, out->desc.height);
  DrawSkybox();
  DrawEntities(inputs, *out);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glDisable(GL_DEPTH_TEST);
}
auto GLRenderer::PickEntity(const int x, const int y) -> EntityID {
  const auto out = GetTarget("SceneMulti");
  if (!out || out->idTexture == 0)
    return EntityID::Invalid;
  if (x < 0 || y < 0 || x >= out->desc.width || y >= out->desc.height)
    return EntityID::Invalid;
  if (pickFramebuffer == 0) {
    glCreateFramebuffers(1, &pickFramebuffer);
    glCreateTextures(GL_TEXTURE_2D, 1, &pickTexture);
    glTextureStorage2D(pickTexture, 1, GL_RGBA8, 1, 1);
    glNamedFramebufferTexture(pickFramebuffer, GL_COLOR_ATTACHMENT0, pickTexture, 0);
  }
  const auto flippedY = out->desc.height - 1 - y;
  // A blit out of a multisampled buffer resolves, and a resolve averages: the average of two ids is
  // a third that belongs to no entity, which is what made a click along an object's silhouette
  // select nothing. So the samples are fetched by a shader instead, and only a target that has one
  // sample -- where a blit is an exact copy and there is nothing to choose between -- still blits.
  // See `pick.frag`.
  auto *pickShader = out->desc.samples > 1 ? GetShader("Pick") : nullptr;
  const auto *frame = pickShader ? GetPrimitive("Frame") : nullptr;
  if (pickShader && frame) {
    glBindFramebuffer(GL_FRAMEBUFFER, pickFramebuffer);
    glDrawBuffer(GL_COLOR_ATTACHMENT0);
    glViewport(0, 0, 1, 1);
    glDisable(GL_DEPTH_TEST);
    pickShader->Use();
    pickShader->SetTexture("u_idImage", out->idTexture);
    pickShader->SetUniform("u_coordX", x);
    pickShader->SetUniform("u_coordY", flippedY);
    pickShader->SetUniform("u_sampleCount", out->desc.samples);
    pickShader->SetUniform("u_model", glm::mat4(1.f));
    pickShader->Draw(*frame);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
  } else {
    glBindFramebuffer(GL_READ_FRAMEBUFFER, out->framebuffer);
    glReadBuffer(GL_COLOR_ATTACHMENT1);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, pickFramebuffer);
    glDrawBuffer(GL_COLOR_ATTACHMENT0);
    glBlitFramebuffer(x, flippedY, x + 1, flippedY + 1, 0, 0, 1, 1, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
  }
  unsigned char pixel[4];
  glGetTextureImage(pickTexture, 0, GL_RGBA, GL_UNSIGNED_BYTE, sizeof(pixel), pixel);
  const auto value = static_cast<uint32_t>(pixel[0]) | static_cast<uint32_t>(pixel[1]) << 8 | static_cast<uint32_t>(pixel[2]) << 16;
  if (value == ENTITY_ID_ENCODED_INVALID)
    return EntityID::Invalid;
  return EntityID{static_cast<long long>(value)};
}
auto GLRenderer::CaptureSceneColor(const GLRenderTarget &target) -> unsigned int {
  const auto width = target.desc.width;
  const auto height = target.desc.height;
  if (width <= 0 || height <= 0)
    return 0;
  if (sceneColorWidth != width || sceneColorHeight != height) {
    if (sceneColorFramebuffer)
      glDeleteFramebuffers(1, &sceneColorFramebuffer);
    if (sceneColorTexture)
      glDeleteTextures(1, &sceneColorTexture);
    glCreateFramebuffers(1, &sceneColorFramebuffer);
    glCreateTextures(GL_TEXTURE_2D, 1, &sceneColorTexture);
    glTextureStorage2D(sceneColorTexture, 1, TargetFormatToGL(target.desc.format).internal, width, height);
    glTextureParameteri(sceneColorTexture, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureParameteri(sceneColorTexture, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTextureParameteri(sceneColorTexture, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(sceneColorTexture, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glNamedFramebufferTexture(sceneColorFramebuffer, GL_COLOR_ATTACHMENT0, sceneColorTexture, 0);
    sceneColorWidth = width;
    sceneColorHeight = height;
  }
  glBindFramebuffer(GL_READ_FRAMEBUFFER, target.framebuffer);
  glReadBuffer(GL_COLOR_ATTACHMENT0);
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, sceneColorFramebuffer);
  glDrawBuffer(GL_COLOR_ATTACHMENT0);
  glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
  glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
  glBindFramebuffer(GL_FRAMEBUFFER, target.framebuffer);
  glViewport(0, 0, width, height);
  return sceneColorTexture;
}
auto GLRenderer::DrawEntities(std::span<std::string> inputs, const GLRenderTarget &target) -> void {
  KUKI_PROFILE_SCOPE("DrawEntities");
  auto scene = GetScene();
  if (!scene)
    return;
  auto camera = scene->GetCamera();
  std::vector<GLDeferredDraw> deferred;
  for (const auto &batch : CollectBatches(*scene, camera)) {
    if (batch.transforms.empty())
      continue;
    const auto blended = batch.material->fallback.alphaMode == AlphaMode::Blend;
    if (blended || batch.material->fallback.transmission > .0f) {
      for (size_t index = 0; index < batch.transforms.size(); ++index) {
        const glm::vec3 position{batch.transforms[index][3]};
        deferred.push_back({batch.mesh, batch.material, batch.fallbacks[index], batch.transforms[index], {}, batch.entityIds[index], blended, camera ? glm::distance(position, camera->position) : 0.f});
      }
      continue;
    }
    DrawEntitiesInstanced(inputs, *batch.mesh, *batch.material, batch.fallbacks, batch.transforms, batch.entityIds);
  }
  DrawSkinnedEntities(inputs, deferred);
  if (deferred.empty())
    return;
  std::sort(deferred.begin(), deferred.end(), [](const GLDeferredDraw &first, const GLDeferredDraw &second) { return first.depth > second.depth; });
  const auto refracts = std::any_of(deferred.begin(), deferred.end(), [](const GLDeferredDraw &draw) { return draw.material->fallback.transmission > .0f; });
  const auto sceneColor = refracts ? CaptureSceneColor(target) : 0u;
  auto blending = false;
  for (const auto &draw : deferred) {
    if (draw.blended != blending) {
      blending = draw.blended;
      if (blending) {
        glEnable(GL_BLEND);
        glDepthMask(GL_FALSE);
      } else {
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
      }
    }
    if (draw.bones.empty())
      DrawEntitiesInstanced(inputs, *draw.mesh, *draw.material, {draw.fallback}, {draw.transform}, {draw.entityId}, sceneColor);
    else
      DrawSkinnedMesh(inputs, *draw.mesh, *draw.material, draw.bones, draw.entityId, sceneColor);
  }
  if (blending) {
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
  }
}
auto GLRenderer::CollectBatches(Scene &scene, const Camera *camera) -> std::span<const GLDrawBatch> {
  KUKI_PROFILE_SCOPE("CollectBatches");
  // how many pooled batches this call has claimed; the rest is last call's, left alone for its capacity
  size_t used = 0;
  batchLookup.clear();
  scene.ForEachEntity<GLMesh, GLMaterial, Transform, Optional<BoundingBox>>([&](const EntityID id, const GLMesh *mesh, const GLMaterial *material, const Transform *transform, const BoundingBox *bounds) {
    if (mesh->vao == 0 || mesh->skinned)
      return;
    // an entity with no bounds cannot be tested, so it is drawn; over-drawing is the safe way to be wrong
    if (camera && bounds && *bounds && !camera->IntersectsFrustum(bounds->GetWorldBounds(transform->world)))
      return;
    const GLMeshMat key{.mesh = *mesh, .material = *material};
    auto it = batchLookup.find(key);
    if (it == batchLookup.end()) {
      if (used == batchPool.size())
        batchPool.emplace_back();
      it = batchLookup.emplace(key, used++).first;
      auto &claimed = batchPool[it->second];
      claimed.mesh = mesh;
      claimed.material = material;
      // clear, never shrink: the capacity is the whole point of pooling these
      claimed.transforms.clear();
      claimed.fallbacks.clear();
      claimed.entityIds.clear();
    }
    auto &batch = batchPool[it->second];
    batch.transforms.push_back(transform->world);
    batch.fallbacks.push_back(material->fallback);
    batch.entityIds.push_back(static_cast<uint32_t>(static_cast<long long>(id)));
  });
  return std::span(batchPool).first(used);
}
auto GLRenderer::DrawSkinnedEntities(std::span<std::string> inputs, std::vector<GLDeferredDraw> &deferred) -> void {
  KUKI_PROFILE_SCOPE("DrawSkinnedEntities");
  auto scene = GetScene();
  if (!scene)
    return;
  auto camera = scene->GetCamera();
  if (!camera)
    return;
  scene->ForEachEntity<ModelMeshHandle, GLMesh, GLMaterial, Transform, Optional<BoundingBox>>([&](const EntityID id, const ModelMeshHandle *handle, const GLMesh *mesh, const GLMaterial *material, const Transform *transform, const BoundingBox *bounds) {
    if (!mesh->skinned || mesh->vao == 0)
      return;
    if (bounds && *bounds && !camera->IntersectsFrustum(bounds->GetWorldBounds(transform->world)))
      return;
    auto modelAsset = app.GetAsset<ModelAsset>(handle->modelAssetId);
    if (!modelAsset || handle->meshIndex >= modelAsset->meshes.size())
      return;
    const auto &modelMesh = modelAsset->meshes[handle->meshIndex];
    Skeleton *skeleton{};
    auto ancestorId = id;
    for (auto i = 0; i < 64 && ancestorId; ++i) {
      if (auto skel = scene->GetEntityComponent<Skeleton>(ancestorId); skel) {
        skeleton = skel;
        break;
      }
      ancestorId = scene->GetParent(ancestorId);
    }
    std::vector<glm::mat4> boneMatrices(modelMesh.bones.size(), glm::mat4(1.f));
    if (skeleton)
      for (auto i = 0u; i < modelMesh.bones.size(); ++i) {
        const auto &bone = modelMesh.bones[i];
        if (bone.nodeIndex >= skeleton->nodeEntities.size())
          continue;
        const auto boneEntityId = skeleton->nodeEntities[bone.nodeIndex];
        if (!boneEntityId)
          continue;
        if (auto boneTransform = scene->GetEntityComponent<Transform>(boneEntityId); boneTransform)
          boneMatrices[i] = boneTransform->world * bone.offsetMatrix;
      }
    const auto entityId = static_cast<uint32_t>(static_cast<long long>(id));
    const auto blended = material->fallback.alphaMode == AlphaMode::Blend;
    if (blended || material->fallback.transmission > .0f) {
      const glm::vec3 position{transform->world[3]};
      deferred.push_back({mesh, material, material->fallback, transform->world, std::move(boneMatrices), entityId, blended, glm::distance(position, camera->position)});
      return;
    }
    DrawSkinnedMesh(inputs, *mesh, *material, boneMatrices, entityId);
  });
}
auto GLRenderer::DrawSkinnedMesh(std::span<std::string> inputs, const GLMesh &mesh, const GLMaterial &material, std::span<const glm::mat4> bones, const uint32_t entityId, const unsigned int sceneColor) -> void {
  auto scene = GetScene();
  if (!scene)
    return;
  auto camera = scene->GetCamera();
  if (!camera)
    return;
  auto shader = GetShader("LitSkinned");
  if (!shader)
    return;
  const auto in = GetTarget(inputs, "ShadowMap");
  const auto spotIn = GetTarget(inputs, "SpotShadowMap");
  if (!in || !spotIn)
    return;
  const auto boneBufferId = CreateBuffer("BoneTransformBuffer");
  const auto materialBufferId = CreateBuffer("MaterialBuffer");
  const auto cameraBufferId = CreateBuffer("CameraBuffer", sizeof(CameraTransform));
  const auto entityIdBufferId = CreateBuffer("EntityIdBuffer");
  const auto boneBuffer = GetBuffer(boneBufferId);
  const auto materialBuffer = GetBuffer(materialBufferId);
  const auto cameraBuffer = GetBuffer(cameraBufferId);
  const auto entityIdBuffer = GetBuffer(entityIdBufferId);
  if (!boneBuffer || !materialBuffer || !cameraBuffer || !entityIdBuffer)
    return;
  shader->Use();
  shader->SetCamera(*camera, cameraBuffer->id);
  auto skybox = scene->GetAnyComponent<GLSkybox>();
  shader->SetSkybox(skybox);
  shader->SetIndirectLighting(indirect);
  std::vector<Light> lights;
  auto hasDirLight = false;
  scene->ForEachEntity<Light>([&](const EntityID, const Light *light) {
    if (light->type == LightType::Directional)
      hasDirLight = true;
    lights.push_back(*light);
  });
  shader->SetLighting(lights);
  if (hasDirLight) {
    shader->SetUniform("u_dirLight.view", shadowView);
    shader->SetUniform("u_dirLight.projection", shadowProjection);
  }
  shader->SetTexture("u_shadowMap", in->texture);
  for (auto i = 0u; i < spotShadowCount; ++i)
    shader->SetUniform("u_spotLightViewProj[" + std::to_string(i) + "]", spotShadowViewProj[i]);
  shader->SetTexture("u_spotShadowMap", spotIn->texture);
  shader->SetMaterial(material);
  shader->SetTexture("u_sceneColor", sceneColor);
  shader->SetMaterialFallback(mesh, material.fallback, materialBuffer->id);
  shader->SetEntityIds(mesh, std::vector<uint32_t>{entityId}, entityIdBuffer->id);
  shader->SetBoneTransforms(bones, boneBuffer->id);
  shader->Draw(mesh, 1);
}
auto GLRenderer::DrawEntitiesInstanced(std::span<std::string> inputs, const GLMesh &mesh, const GLMaterial &material, const std::vector<MaterialFallback> &fallbacks, const std::vector<glm::mat4> &transforms, const std::vector<uint32_t> &entityIds, const unsigned int sceneColor) -> void {
  auto scene = GetScene();
  if (!scene)
    return;
  auto camera = scene->GetCamera();
  if (!camera)
    return;
  auto materialName = EnumTraits<MaterialType>::GetNames()[static_cast<int>(material.type)];
  auto shader = GetShader(materialName);
  if (!shader)
    return;
  const auto in = GetTarget(inputs, "ShadowMap");
  const auto spotIn = GetTarget(inputs, "SpotShadowMap");
  if (!in || !spotIn)
    return;
  const auto materialBufferId = CreateBuffer("MaterialBuffer");
  const auto transformBufferId = CreateBuffer("TransformBuffer");
  const auto cameraBufferId = CreateBuffer("CameraBuffer", sizeof(CameraTransform));
  const auto entityIdBufferId = CreateBuffer("EntityIdBuffer");
  const auto materialBuffer = GetBuffer(materialBufferId);
  const auto transformBuffer = GetBuffer(transformBufferId);
  const auto cameraBuffer = GetBuffer(cameraBufferId);
  const auto entityIdBuffer = GetBuffer(entityIdBufferId);
  if (!materialBuffer || !transformBuffer || !cameraBuffer || !entityIdBuffer)
    return;
  shader->Use();
  shader->SetCamera(*camera, cameraBuffer->id);
  auto skybox = scene->GetAnyComponent<GLSkybox>();
  shader->SetSkybox(skybox);
  shader->SetIndirectLighting(indirect);
  std::vector<Light> lights;
  auto hasDirLight = false;
  scene->ForEachEntity<Light>([&](const EntityID, const Light *light) {
    if (light->type == LightType::Directional)
      hasDirLight = true;
    lights.push_back(*light);
  });
  shader->SetLighting(lights);
  if (hasDirLight) {
    shader->SetUniform("u_dirLight.view", shadowView);
    shader->SetUniform("u_dirLight.projection", shadowProjection);
  }
  shader->SetTexture("u_shadowMap", in->texture);
  for (auto i = 0u; i < spotShadowCount; ++i)
    shader->SetUniform("u_spotLightViewProj[" + std::to_string(i) + "]", spotShadowViewProj[i]);
  shader->SetTexture("u_spotShadowMap", spotIn->texture);
  shader->SetMaterial(material);
  shader->SetTexture("u_sceneColor", sceneColor);
  shader->SetMaterialFallback(mesh, fallbacks, materialBuffer->id);
  shader->SetTransform(mesh, transforms, transformBuffer->id);
  shader->SetEntityIds(mesh, entityIds, entityIdBuffer->id);
  shader->Draw(mesh, transforms.size());
}
auto GLRenderer::DrawMeshes() -> void {
  auto scene = GetScene();
  if (!scene)
    return;
  const Light *dirLight{};
  scene->ForEachEntity<Light>([&](const EntityID, const Light *light) {
    if (light->type == LightType::Directional)
      dirLight = light;
  });
  if (!dirLight)
    return;
  FitShadowFrustum(*scene, *dirLight);
  DrawOpaqueMeshes(shadowView, shadowProjection);
}
/// @brief Every opaque, unskinned mesh, drawn depth only through the given camera.
///
/// The same geometry a shadow map wants and a depth prepass wants, which differ only in whose eye
/// they are seen from.
auto GLRenderer::DrawOpaqueMeshes(const glm::mat4 &view, const glm::mat4 &projection) -> void {
  auto scene = GetScene();
  if (!scene)
    return;
  std::unordered_map<GLMesh, std::vector<glm::mat4>> meshToTransforms;
  scene->ForEachEntity<GLMesh, Transform>([&](const EntityID id, const GLMesh *mesh, const Transform *transform) {
    if (mesh->vao == 0 || mesh->skinned)
      return;
    if (const auto material = scene->GetEntityComponent<GLMaterial>(id); material && (material->fallback.alphaMode == AlphaMode::Blend || material->fallback.transmission > .0f))
      return;
    meshToTransforms[*mesh].push_back(transform->world);
  });
  for (const auto &[mesh, transforms] : meshToTransforms)
    DrawMeshesInstanced(mesh, transforms, view, projection);
}
auto GLRenderer::CreateDepthPrepass(std::span<std::string>, std::span<std::string> outputs) -> void {
  auto scene = GetScene();
  if (!scene)
    return;
  const auto out = GetTarget(outputs, "SceneDepth");
  auto camera = scene->GetCamera();
  if (!out || !camera)
    return;
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LESS);
  glDepthMask(GL_TRUE);
  glBindFramebuffer(GL_FRAMEBUFFER, out->framebuffer);
  glViewport(0, 0, out->desc.width, out->desc.height);
  glClear(GL_DEPTH_BUFFER_BIT);
  DrawOpaqueMeshes(camera->transform.view, camera->transform.projection);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
auto GLRenderer::DrawMeshesInstanced(const GLMesh &mesh, const std::vector<glm::mat4> &transforms, const glm::mat4 &view, const glm::mat4 &projection) -> void {
  auto shader = GetShader("ShadowMap");
  if (!shader)
    return;
  const auto transformBufferId = CreateBuffer("TransformBuffer");
  const auto cameraBufferId = CreateBuffer("CameraBuffer", sizeof(CameraTransform));
  const auto transformBuffer = GetBuffer(transformBufferId);
  const auto cameraBuffer = GetBuffer(cameraBufferId);
  if (!transformBuffer || !cameraBuffer)
    return;
  Camera camera{};
  camera.transform.view = view;
  camera.transform.projection = projection;
  shader->Use();
  shader->SetCamera(camera, cameraBuffer->id);
  shader->SetTransform(mesh, transforms, transformBuffer->id);
  shader->Draw(mesh, transforms.size());
}
auto GLRenderer::FitShadowFrustum(Scene &scene, const Light &light) -> void {
  KUKI_PROFILE_SCOPE("FitShadowFrustum");
  BoundingBox sceneBounds{};
  scene.ForEachEntity<BoundingBox, Transform>([&](const EntityID, const BoundingBox *box, const Transform *transform) {
    const auto worldBounds = box->GetWorldBounds(transform->world);
    sceneBounds.min = glm::min(sceneBounds.min, worldBounds.min);
    sceneBounds.max = glm::max(sceneBounds.max, worldBounds.max);
  });
  if (!sceneBounds) {
    shadowView = light.GetView();
    Camera fallback{.type = CameraType::Orthographic};
    fallback.nearPlane = light.nearPlane;
    fallback.farPlane = light.farPlane;
    fallback.orthoSize = light.orthoSize;
    fallback.SetTransform(light.GetTransform());
    shadowProjection = fallback.transform.projection;
    return;
  }
  const auto center = (sceneBounds.min + sceneBounds.max) * .5f;
  const auto radius = glm::length(sceneBounds.max - sceneBounds.min) * .5f;
  shadowView = glm::lookAt(center - light.forward * radius * 2.f, center, light.up);
  auto boundsMin = glm::vec3(std::numeric_limits<float>::max());
  auto boundsMax = glm::vec3(std::numeric_limits<float>::lowest());
  const glm::vec3 corners[8] = {
    {sceneBounds.min.x, sceneBounds.min.y, sceneBounds.min.z},
    {sceneBounds.max.x, sceneBounds.min.y, sceneBounds.min.z},
    {sceneBounds.min.x, sceneBounds.max.y, sceneBounds.min.z},
    {sceneBounds.max.x, sceneBounds.max.y, sceneBounds.min.z},
    {sceneBounds.min.x, sceneBounds.min.y, sceneBounds.max.z},
    {sceneBounds.max.x, sceneBounds.min.y, sceneBounds.max.z},
    {sceneBounds.min.x, sceneBounds.max.y, sceneBounds.max.z},
    {sceneBounds.max.x, sceneBounds.max.y, sceneBounds.max.z}};
  for (const auto &corner : corners) {
    const auto viewSpace = glm::vec3(shadowView * glm::vec4(corner, 1.f));
    boundsMin = glm::min(boundsMin, viewSpace);
    boundsMax = glm::max(boundsMax, viewSpace);
  }
  constexpr auto PADDING = .5f;
  const auto nearPlane = std::max(.01f, -boundsMax.z - PADDING);
  const auto farPlane = -boundsMin.z + PADDING;
  shadowProjection = glm::ortho(boundsMin.x - PADDING, boundsMax.x + PADDING, boundsMin.y - PADDING, boundsMax.y + PADDING, nearPlane, farPlane);
}
auto GLRenderer::CreateSpotShadowMap(std::span<std::string> inputs, std::span<std::string> outputs) -> void {
  auto scene = GetScene();
  if (!scene)
    return;
  const auto out = GetTarget(outputs, "SpotShadowMap");
  if (!out)
    return;
  std::vector<const Light *> spotLights;
  scene->ForEachEntity<Light>([&](const EntityID, const Light *light) {
    if (light->type == LightType::Spot && spotLights.size() < MAX_SPOT_SHADOW_LIGHTS)
      spotLights.push_back(light);
  });
  spotShadowCount = static_cast<unsigned int>(spotLights.size());
  if (spotShadowCount == 0)
    return;
  std::unordered_map<GLMesh, std::vector<glm::mat4>> meshToTransforms;
  scene->ForEachEntity<GLMesh, Transform>([&](const EntityID id, const GLMesh *mesh, const Transform *transform) {
    if (mesh->vao == 0 || mesh->skinned)
      return;
    if (const auto material = scene->GetEntityComponent<GLMaterial>(id); material && (material->fallback.alphaMode == AlphaMode::Blend || material->fallback.transmission > .0f))
      return;
    meshToTransforms[*mesh].push_back(transform->world);
  });
  glEnable(GL_DEPTH_TEST);
  glBindFramebuffer(GL_FRAMEBUFFER, out->framebuffer);
  glViewport(0, 0, out->desc.width, out->desc.height);
  for (auto i = 0u; i < spotShadowCount; ++i) {
    glm::mat4 view{1.f};
    glm::mat4 projection{1.f};
    FitSpotShadowFrustum(*spotLights[i], view, projection);
    spotShadowViewProj[i] = projection * view;
    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, out->texture, 0, static_cast<int>(i));
    glClear(GL_DEPTH_BUFFER_BIT);
    for (const auto &[mesh, transforms] : meshToTransforms)
      DrawMeshesInstanced(mesh, transforms, view, projection);
  }
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
auto GLRenderer::FitSpotShadowFrustum(const Light &light, glm::mat4 &view, glm::mat4 &projection) -> void {
  constexpr auto MAX_FOV = glm::radians(170.f);
  const auto fov = std::min(MAX_FOV, 2.f * std::acos(std::clamp(light.outerCutoff, -1.f, 1.f)) * 1.05f);
  view = glm::lookAt(light.position, light.position + light.forward, light.up);
  projection = glm::perspective(fov, 1.f, light.nearPlane, light.farPlane);
}
auto GLRenderer::DrawSkybox() -> void {
  auto scene = GetScene();
  if (!scene)
    return;
  auto camera = scene->GetCamera();
  if (!camera)
    return;
  auto shader = GetShader("Skybox");
  if (!shader)
    return;
  const auto mesh = GetPrimitive("Frame");
  if (!mesh)
    return;
  const auto cameraBufferId = CreateBuffer("CameraBuffer", sizeof(CameraTransform));
  const auto cameraBuffer = GetBuffer(cameraBufferId);
  if (!cameraBuffer)
    return;
  shader->Use();
  shader->SetCamera(*camera, cameraBuffer->id);
  auto skybox = scene->GetAnyComponent<GLSkybox>();
  shader->SetTexture("u_skybox", skybox ? skybox->skybox : 0);
  shader->SetUniform("u_useGradient", skybox != nullptr);
  shader->SetUniform("u_useSkybox", skybox != nullptr && skybox->skybox > 0);
  shader->SetUniform("u_background", indirect.backgroundColor);
  glDepthFunc(GL_LEQUAL);
  glDepthMask(GL_FALSE);
  shader->Draw(*mesh);
  glDepthMask(GL_TRUE);
  glDepthFunc(GL_LESS);
}
auto GLRenderer::BorrowBuffer(const int &size) -> unsigned int {
  return bufferPool.Request(size);
}
auto GLRenderer::BorrowFramebuffer() -> unsigned int {
  return framebufferPool.Request();
}
auto GLRenderer::BorrowRenderbuffer(const TargetDescription &desc) -> unsigned int {
  return renderbufferPool.Request(desc);
}
auto GLRenderer::BorrowTexture(const TargetDescription &desc) -> unsigned int {
  return texturePool.Request(desc);
}
auto GLRenderer::Clear() -> void {
  // Nothing to delete against, and nothing that may be attempted. Every renderer the system built
  // is cleared at shutdown, active or not, and these calls need a current context rather than
  // merely a live one -- so a backend that is not the one presenting drops its handles instead of
  // naming them to whatever context happens to be current. Only its own can free them, and if that
  // context is gone the driver has already taken them back.
  if (!dynamic_cast<GLContext *>(app.GetGraphicsContext())) {
    resourceRegistry.Clear();
    assetIdToResourceId.clear();
    assetIdToPreviewId.clear();
    return;
  }
  // Everything below is deleted rather than returned to a pool. What a pool is for is the next
  // request, and after this there is not going to be one: this runs when the backend is being torn
  // down, and holding a texture for a scene that will never be loaded is the leak with extra steps.
  //
  // Deleting rather than returning also keeps the books straight for free. `Clear` on a pool drops
  // its buckets outright, so whatever was lent out stops being counted at the same moment it stops
  // existing, and nothing is left claiming to be in use by a scene that is gone.
  const auto DeleteTexture = [](unsigned int &id) {
    if (id)
      glDeleteTextures(1, &id);
    id = 0;
  };
  const auto DeleteFramebuffer = [](unsigned int &id) {
    if (id)
      glDeleteFramebuffers(1, &id);
    id = 0;
  };
  resourceRegistry.ForEach<GLTexture>([&](GLTexture &texture) {
    DeleteTexture(texture.id);
  });
  resourceRegistry.ForEach<GLSkybox>([&](GLSkybox &skybox) {
    // Four textures from one asset, and each of them borrowed under a description of its own, so
    // there is no single key to hand them back under even if handing them back were wanted here.
    DeleteTexture(skybox.skybox);
    DeleteTexture(skybox.irradiance);
    DeleteTexture(skybox.prefilter);
    DeleteTexture(skybox.brdf);
  });
  resourceRegistry.ForEach<GLRenderTarget>([&](GLRenderTarget &target) {
    DeleteFramebuffer(target.framebuffer);
    if (target.renderbuffer)
      glDeleteRenderbuffers(1, &target.renderbuffer);
    target.renderbuffer = 0;
    DeleteTexture(target.texture);
    DeleteTexture(target.idTexture);
  });
  resourceRegistry.ForEach<GLMesh>([](GLMesh &mesh) {
    if (mesh.vao)
      glDeleteVertexArrays(1, &mesh.vao);
    if (mesh.ebo)
      glDeleteBuffers(1, &mesh.ebo);
    if (mesh.vbo)
      glDeleteBuffers(1, &mesh.vbo);
    mesh.vao = 0;
    mesh.ebo = 0;
    mesh.vbo = 0;
  });
  resourceRegistry.ForEach<GLBuffer>([](GLBuffer &buffer) {
    if (buffer.id)
      glDeleteBuffers(1, &buffer.id);
    buffer.id = 0;
  });
  // By hand rather than through the registry, because the overlay's buffer never went in one: it
  // belongs to the renderer rather than to any asset, and nothing but the overlay pass draws it.
  if (overlayBuffer.vao)
    glDeleteVertexArrays(1, &overlayBuffer.vao);
  if (overlayBuffer.vbo)
    glDeleteBuffers(1, &overlayBuffer.vbo);
  overlayBuffer = {};
  // A program is not a pooled object and never was: it is compiled once from source the engine
  // carries, so the only place it can be released is here.
  const auto DeleteProgram = [](GLShaderBase &shader) {
    if (shader.id)
      glDeleteProgram(shader.id);
    shader.id = 0;
  };
  resourceRegistry.ForEach<GLComputeShader>(DeleteProgram);
  resourceRegistry.ForEach<GLLitShader>(DeleteProgram);
  resourceRegistry.ForEach<GLUnlitShader>(DeleteProgram);
  resourceRegistry.Clear();
  assetIdToResourceId.clear();
  assetIdToPreviewId.clear();
  // Held directly rather than through the registry, because each is a fixed part of the pipeline
  // rather than something an asset brought with it -- and so each has to be named here by hand.
  DeleteFramebuffer(pickFramebuffer);
  DeleteTexture(pickTexture);
  DeleteFramebuffer(outlineIdFramebuffer);
  DeleteTexture(outlineIdTexture);
  outlineIdWidth = 0;
  outlineIdHeight = 0;
  DeleteFramebuffer(sceneColorFramebuffer);
  DeleteTexture(sceneColorTexture);
  sceneColorWidth = 0;
  sceneColorHeight = 0;
  spotShadowCount = 0;
  // Last, so that anything the walks above returned rather than deleted still goes.
  bufferPool.Clear();
  framebufferPool.Clear();
  renderbufferPool.Clear();
  texturePool.Clear();
  framesSinceCollect = 0;
}
auto GLRenderer::CreateBuffer(const std::string &name, const int &size) -> EntityID {
  EntityID id{};
  if (auto id_ = resourceRegistry.GetID(name); id_)
    id = id_;
  else
    id = resourceRegistry.Create(name);
  auto buffer = resourceRegistry.AddComponent<GLBuffer>(id);
  if (buffer->id == 0) {
    buffer->id = BorrowBuffer(size);
    spdlog::info("[GLRenderer] Created buffer: {}", name);
  }
  return id;
}
auto GLRenderer::CreateTarget(const TargetDescription &desc, const std::string &name) -> EntityID {
  EntityID id{};
  if (auto id_ = resourceRegistry.GetID(name); id_)
    id = id_;
  else
    id = resourceRegistry.Create(name);
  auto renderTarget = resourceRegistry.AddComponent<GLRenderTarget>(id);
  if (renderTarget->framebuffer == 0) {
    renderTarget->framebuffer = BorrowFramebuffer();
    spdlog::info("[GLRenderer] Created render target: {}", name);
  }
  if (renderTarget->renderbuffer == 0)
    renderTarget->renderbuffer = BorrowRenderbuffer(desc);
  if (renderTarget->texture == 0)
    renderTarget->texture = BorrowTexture(desc);
  if (desc.pickingBuffer && renderTarget->idTexture == 0)
    renderTarget->idTexture = BorrowTexture({.format = TargetFormat::RGBA8, .type = desc.type, .width = desc.width, .height = desc.height, .samples = desc.samples});
  renderTarget->desc = desc;
  glBindFramebuffer(GL_FRAMEBUFFER, renderTarget->framebuffer);
  const auto attachment = desc.format == TargetFormat::DEPTH ? GL_DEPTH_ATTACHMENT : GL_COLOR_ATTACHMENT0;
  const auto textureTarget = desc.samples > 1 ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D;
  if (desc.type == TargetType::Texture2DArray)
    glFramebufferTextureLayer(GL_FRAMEBUFFER, attachment, renderTarget->texture, 0, 0);
  else
    glFramebufferTexture2D(GL_FRAMEBUFFER, attachment, textureTarget, renderTarget->texture, 0);
  if (desc.pickingBuffer)
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, textureTarget, renderTarget->idTexture, 0);
  if (attachment != GL_DEPTH_ATTACHMENT) {
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, renderTarget->renderbuffer);
    if (desc.pickingBuffer) {
      constexpr GLenum drawBuffers[] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
      glDrawBuffers(2, drawBuffers);
    }
  } else {
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
  }
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  return id;
}
auto GLRenderer::CreateTexture(const TargetDescription &desc, const std::string &name) -> EntityID {
  EntityID id{};
  if (auto id_ = resourceRegistry.GetID(name); id_)
    id = id_;
  else
    id = resourceRegistry.Create(name);
  auto texture = resourceRegistry.AddComponent<GLTexture>(id);
  if (texture->id == 0) {
    texture->id = BorrowTexture(desc);
    spdlog::info("[GLRenderer] Created texture: {}", name);
  }
  return id;
}
auto GLRenderer::GetBuffer(const EntityID id) -> GLBuffer * {
  return resourceRegistry.GetComponent<GLBuffer>(id);
}
auto GLRenderer::GetBuffer(const std::string &name) -> GLBuffer * {
  return resourceRegistry.GetComponent<GLBuffer>(name);
}
auto GLRenderer::GetCompute(const std::string &name) -> GLComputeShader * {
  return resourceRegistry.GetComponent<GLComputeShader>(name);
}
auto GLRenderer::GetPrimitive(const std::string &name) -> GLMesh * {
  return resourceRegistry.GetComponent<GLMesh>(name);
}
auto GLRenderer::GetAssetPreviewId(const AssetID id) const -> EntityID {
  if (auto it = assetIdToPreviewId.find(id); it != assetIdToPreviewId.end())
    return it->second;
  return EntityID::Invalid;
}
auto GLRenderer::GetAssetResourceId(const AssetID id) const -> EntityID {
  if (auto it = assetIdToResourceId.find(id); it != assetIdToResourceId.end())
    return it->second;
  return EntityID::Invalid;
}
auto GLRenderer::GetDefaultMaterialAssetId() -> AssetID {
  auto asset = app.GetAsset("DefaultLit");
  return asset ? asset->id : AssetID{};
}
auto GLRenderer::GetDefaultMeshAssetId() -> AssetID {
  auto asset = app.GetAsset("Cube");
  return asset ? asset->id : AssetID{};
}
auto GLRenderer::GetPreviewSize() const -> int {
  return previewSize;
}
auto GLRenderer::GetResourceID(const std::string &name) const -> EntityID {
  return resourceRegistry.GetID(name);
}
auto GLRenderer::GetShader(const std::string &name) -> GLShader * {
  const auto id = resourceRegistry.GetID(name);
  if (!id)
    return nullptr;
  if (auto shader = resourceRegistry.GetComponent<GLLitShader>(id); shader)
    return shader;
  return resourceRegistry.GetComponent<GLUnlitShader>(id);
}
auto GLRenderer::GetTarget(const EntityID id) -> GLRenderTarget * {
  return resourceRegistry.GetComponent<GLRenderTarget>(id);
}
auto GLRenderer::GetTarget(const std::string &name) -> GLRenderTarget * {
  return resourceRegistry.GetComponent<GLRenderTarget>(name);
}
auto GLRenderer::GetTarget(std::span<std::string> names, const std::string &name) -> GLRenderTarget * {
  if (std::find(names.begin(), names.end(), name) == names.end())
    return nullptr;
  return GetTarget(name);
}
auto GLRenderer::GetTexture(const EntityID id) -> GLTexture * {
  return resourceRegistry.GetComponent<GLTexture>(id);
}
auto GLRenderer::GetTexture(const std::string &name) -> GLTexture * {
  return resourceRegistry.GetComponent<GLTexture>(name);
}
auto GLRenderer::LoadAsset(Asset &asset) -> void {
  if (asset.Is<MaterialAsset>())
    LoadAsset<MaterialAsset>(*asset.As<MaterialAsset>());
  else if (asset.Is<MeshAsset>())
    LoadAsset<MeshAsset>(*asset.As<MeshAsset>());
  else if (asset.Is<ModelAsset>())
    LoadAsset<ModelAsset>(*asset.As<ModelAsset>());
  else if (asset.Is<ShaderAsset>())
    LoadAsset<ShaderAsset>(*asset.As<ShaderAsset>());
  else if (asset.Is<TextureAsset>())
    LoadAsset<TextureAsset>(*asset.As<TextureAsset>());
}
auto GLRenderer::LoadAsset(const AssetID id) -> void {
  if (auto asset = app.GetAsset(id); asset)
    LoadAsset(*asset);
}
auto GLRenderer::LoadAssets(const AssetType type) -> void {
  app.ForEachAsset(type, [&](Asset *asset) {
    LoadAsset(*asset);
  });
}
auto GLRenderer::LoadScene(Scene &scene) -> void {
  // Picked up once a frame here rather than read in the pass that needs it, because the post chain
  // is handed target names and nothing else. A scene with no camera keeps whatever was last set,
  // which is the same thing it draws with.
  if (const auto camera = scene.GetCamera(); camera)
    exposure = camera->GetExposureStops();
  std::vector<std::pair<EntityID, EntityID>> materialsToPopulate;
  scene.ForEachEntity<MaterialHandle>([&](const EntityID id, MaterialHandle *materialHandle) {
    if (!materialHandle || materialHandle->resourceId)
      return;
    const auto pending = materialHandle->assetId && !app.IsAssetLoaded(materialHandle->assetId);
    const auto targetId = materialHandle->assetId && !pending ? materialHandle->assetId : GetDefaultMaterialAssetId();
    if (auto materialAsset = app.GetAsset<MaterialAsset>(targetId); materialAsset) {
      LoadAsset<MaterialAsset>(*materialAsset);
      const auto resourceId = GetAssetResourceId(materialAsset->id);
      scene.AddEntityComponent<GLMaterial>(id);
      materialsToPopulate.emplace_back(id, resourceId);
      if (!pending)
        materialHandle->resourceId = resourceId;
    }
  });
  for (const auto &[id, resourceId] : materialsToPopulate)
    if (auto material = scene.GetEntityComponent<GLMaterial>(id); material)
      if (auto material_ = resourceRegistry.GetComponent<GLMaterial>(resourceId); material_)
        *material = *material_;
  std::vector<std::pair<EntityID, EntityID>> meshesToPopulate;
  scene.ForEachEntity<MeshHandle>([&](const EntityID id, MeshHandle *meshHandle) {
    if (!meshHandle || meshHandle->resourceId)
      return;
    const auto pending = meshHandle->assetId && !app.IsAssetLoaded(meshHandle->assetId);
    const auto targetId = meshHandle->assetId && !pending ? meshHandle->assetId : GetDefaultMeshAssetId();
    if (auto meshAsset = app.GetAsset<MeshAsset>(targetId); meshAsset) {
      LoadAsset<MeshAsset>(*meshAsset);
      const auto resourceId = GetAssetResourceId(meshAsset->id);
      scene.AddEntityComponent<GLMesh>(id);
      meshesToPopulate.emplace_back(id, resourceId);
      UpdatePlaceholderScale(scene, id, pending);
      if (!pending)
        meshHandle->resourceId = resourceId;
    }
  });
  for (const auto &[id, resourceId] : meshesToPopulate)
    if (auto mesh = scene.GetEntityComponent<GLMesh>(id); mesh)
      if (auto mesh_ = resourceRegistry.GetComponent<GLMesh>(resourceId); mesh_)
        *mesh = *mesh_;
  std::vector<std::pair<EntityID, EntityID>> modelMaterialsToPopulate;
  scene.ForEachEntity<ModelMaterialHandle>([&](const EntityID id, ModelMaterialHandle *materialHandle) {
    if (!materialHandle || materialHandle->resourceId)
      return;
    if (app.IsAssetLoaded(materialHandle->modelAssetId)) {
      if (auto modelAsset = app.GetAsset<ModelAsset>(materialHandle->modelAssetId); modelAsset) {
        materialHandle->resourceId = LoadModelMaterial(*modelAsset, materialHandle->materialIndex);
        scene.AddEntityComponent<GLMaterial>(id);
        modelMaterialsToPopulate.emplace_back(id, materialHandle->resourceId);
      }
      return;
    }
    if (auto materialAsset = app.GetAsset<MaterialAsset>(GetDefaultMaterialAssetId()); materialAsset) {
      LoadAsset<MaterialAsset>(*materialAsset);
      scene.AddEntityComponent<GLMaterial>(id);
      modelMaterialsToPopulate.emplace_back(id, GetAssetResourceId(materialAsset->id));
    }
  });
  for (const auto &[id, resourceId] : modelMaterialsToPopulate)
    if (auto material = scene.GetEntityComponent<GLMaterial>(id); material)
      if (auto material_ = resourceRegistry.GetComponent<GLMaterial>(resourceId); material_)
        *material = *material_;
  std::vector<std::pair<EntityID, EntityID>> modelMeshesToPopulate;
  scene.ForEachEntity<ModelMeshHandle>([&](const EntityID id, ModelMeshHandle *meshHandle) {
    if (!meshHandle || meshHandle->resourceId)
      return;
    if (app.IsAssetLoaded(meshHandle->modelAssetId)) {
      if (auto modelAsset = app.GetAsset<ModelAsset>(meshHandle->modelAssetId); modelAsset) {
        meshHandle->resourceId = LoadModelMesh(*modelAsset, meshHandle->meshIndex);
        scene.AddEntityComponent<GLMesh>(id);
        modelMeshesToPopulate.emplace_back(id, meshHandle->resourceId);
        UpdatePlaceholderScale(scene, id, false);
      }
      return;
    }
    if (auto meshAsset = app.GetAsset<MeshAsset>(GetDefaultMeshAssetId()); meshAsset) {
      LoadAsset<MeshAsset>(*meshAsset);
      scene.AddEntityComponent<GLMesh>(id);
      modelMeshesToPopulate.emplace_back(id, GetAssetResourceId(meshAsset->id));
      UpdatePlaceholderScale(scene, id, true);
    }
  });
  for (const auto &[id, resourceId] : modelMeshesToPopulate)
    if (auto mesh = scene.GetEntityComponent<GLMesh>(id); mesh)
      if (auto mesh_ = resourceRegistry.GetComponent<GLMesh>(resourceId); mesh_)
        *mesh = *mesh_;
  std::vector<std::pair<EntityID, EntityID>> skyboxesToPopulate;
  scene.ForEachEntity<SkyboxHandle>([&](const EntityID id, SkyboxHandle *skyboxHandle) {
    if (!skyboxHandle || skyboxHandle->resourceId)
      return;
    if (!skyboxHandle->assetId || !app.IsAssetLoaded(skyboxHandle->assetId))
      return;
    if (auto textureAsset = app.GetAsset<TextureAsset>(skyboxHandle->assetId); textureAsset) {
      LoadAsset<TextureAsset>(*textureAsset);
      skyboxHandle->resourceId = GetAssetResourceId(textureAsset->id);
      skyboxesToPopulate.emplace_back(id, skyboxHandle->resourceId);
    }
  });
  for (const auto &[id, resourceId] : skyboxesToPopulate)
    if (auto skybox = scene.GetEntityComponent<GLSkybox>(id); skybox)
      if (auto skybox_ = resourceRegistry.GetComponent<GLSkybox>(resourceId); skybox_)
        *skybox = *skybox_;
}
auto GLRenderer::PreviewAsset(const AssetID id) -> GLTexture * {
  LoadAsset(id);
  if (auto asset = app.GetAsset(id); asset) {
    if (asset->Is<MeshAsset>())
      return PreviewAsset<MeshAsset>(*asset->As<MeshAsset>());
    else if (asset->Is<ModelAsset>())
      return PreviewAsset<ModelAsset>(*asset->As<ModelAsset>());
    else if (asset->Is<TextureAsset>())
      return PreviewAsset<TextureAsset>(*asset->As<TextureAsset>());
    else if (asset->Is<MaterialAsset>())
      return PreviewAsset<MaterialAsset>(*asset->As<MaterialAsset>());
  }
  return nullptr;
}
auto GLRenderer::PresentTarget(const std::string &name) -> void {
  const auto *target = GetTarget(name);
  if (!target || !target->framebuffer)
    return;
  const auto *context = app.GetGraphicsContext();
  if (!context)
    return;
  const auto [surfaceWidth, surfaceHeight] = context->GetSurfaceSize();
  if (surfaceWidth <= 0 || surfaceHeight <= 0)
    return;
  // A blit rather than a textured quad, because the source is already display-encoded by the tone
  // mapping pass and the default framebuffer wants exactly those bytes. Linear filtering matters:
  // the render resolution and the window rarely agree, and this is the one place they are reconciled.
  glBindFramebuffer(GL_READ_FRAMEBUFFER, target->framebuffer);
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
  glBlitFramebuffer(0, 0, target->desc.width, target->desc.height, 0, 0, surfaceWidth, surfaceHeight, GL_COLOR_BUFFER_BIT, GL_LINEAR);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
auto GLRenderer::Reset() -> void {
  glClearColor(0.f, 0.f, 0.f, 0.f);
  // The pools are given a round here rather than on a timer of their own, because this is the one
  // place that runs once a frame with a live context and nothing borrowed: the graph calls it before
  // it touches a target, so every resource a frame will use is still in a pool or still lent out
  // from the last one, and neither is in the middle of being handed over.
  if (++framesSinceCollect < POOL_COLLECT_INTERVAL)
    return;
  framesSinceCollect = 0;
  const auto freed = bufferPool.Collect() + framebufferPool.Collect() + renderbufferPool.Collect() + texturePool.Collect();
  if (freed == 0)
    return;
  const auto usage = GetPoolUsage();
  spdlog::debug("[GLRenderer] Pools released {} resources, {} still lent out and {} waiting across {} kinds", freed, usage.inUse, usage.available, usage.keys);
}
auto GLRenderer::SetPreviewSize(const int size) -> void {
  if (size == previewSize || size <= 0)
    return;
  previewSize = size;
  assetIdToPreviewId.clear();
}
auto GLRenderer::SetResolution(const int width, const int height) -> void {
  glViewport(0, 0, width, height);
}
// Nothing to trace: this backend's indirect diffuse is the image-based term and screen space
// occlusion, and both of those already end in a graph target anyone can select. The pass is still
// executed here because the graph is shared between the backends, and one that does nothing costs
// the call it takes to find that out.
auto GLRenderer::TraceProbes(std::span<std::string>, std::span<std::string>) -> void {}
auto GLRenderer::UpdateTarget(const std::string &name, const TargetDescription &desc) -> void {
  const auto id = resourceRegistry.GetID(name);
  if (!id)
    return;
  auto target = resourceRegistry.GetComponent<GLRenderTarget>(id);
  if (!target)
    return;
  // Reallocated in place rather than given back and asked for again, which is cheaper and is why
  // dragging a viewport edge does not churn the pools. The cost is that the pool would otherwise go
  // on counting these against the size they were born at -- one key per width the window has ever
  // been, each holding a resource that is permanently lent out and can therefore never fall idle.
  // Moving the accounting across is what lets the abandoned sizes be collected.
  const auto previous = target->desc;
  const TargetDescription previousId{.format = TargetFormat::RGBA8, .type = previous.type, .width = previous.width, .height = previous.height, .samples = previous.samples};
  const TargetDescription currentId{.format = TargetFormat::RGBA8, .type = desc.type, .width = desc.width, .height = desc.height, .samples = desc.samples};
  target->desc = desc;
  renderbufferPool.Rekey(previous, desc);
  renderbufferPool.Reallocate(desc, target->renderbuffer);
  texturePool.Rekey(previous, desc);
  texturePool.Reallocate(desc, target->texture);
  if (desc.pickingBuffer) {
    texturePool.Rekey(previousId, currentId);
    texturePool.Reallocate(currentId, target->idTexture);
  }
  glBindFramebuffer(GL_FRAMEBUFFER, target->framebuffer);
  const auto attachment = desc.format == TargetFormat::DEPTH ? GL_DEPTH_ATTACHMENT : GL_COLOR_ATTACHMENT0;
  const auto textureTarget = desc.samples > 1 ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D;
  if (desc.type == TargetType::Texture2DArray)
    glFramebufferTextureLayer(GL_FRAMEBUFFER, attachment, target->texture, 0, 0);
  else
    glFramebufferTexture2D(GL_FRAMEBUFFER, attachment, textureTarget, target->texture, 0);
  if (desc.pickingBuffer)
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, textureTarget, target->idTexture, 0);
  if (attachment != GL_DEPTH_ATTACHMENT) {
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, target->renderbuffer);
    if (desc.pickingBuffer) {
      constexpr GLenum drawBuffers[] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
      glDrawBuffers(2, drawBuffers);
    }
  } else {
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
  }
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
auto GLRenderer::CreateIndexBuffer(GLMesh &mesh, const std::vector<unsigned int> &indices) -> void {
  mesh.indexCount = indices.size();
  GLuint indexBuffer;
  glCreateBuffers(1, &indexBuffer);
  mesh.ebo = indexBuffer;
  glNamedBufferData(mesh.ebo, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);
  glVertexArrayElementBuffer(mesh.vao, mesh.ebo);
}
auto GLRenderer::CreateVertexBuffer(GLMesh &mesh, const std::vector<Vertex> &vertices, bool skinned) -> void {
  mesh.vertexCount = vertices.size();
  GLuint vao, vbo;
  glCreateVertexArrays(1, &vao);
  glCreateBuffers(1, &vbo);
  mesh.vao = vao;
  mesh.vbo = vbo;
  auto bindingIndex = 0;
  glNamedBufferData(vbo, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);
  glVertexArrayVertexBuffer(mesh.vao, bindingIndex, vbo, 0, sizeof(Vertex));
  auto attribIndex = 0;
  glVertexArrayAttribFormat(mesh.vao, attribIndex, 3, GL_FLOAT, GL_FALSE, offsetof(Vertex, position));
  glVertexArrayAttribBinding(mesh.vao, attribIndex, bindingIndex);
  glEnableVertexArrayAttrib(mesh.vao, attribIndex);
  attribIndex++;
  glVertexArrayAttribFormat(mesh.vao, attribIndex, 3, GL_FLOAT, GL_FALSE, offsetof(Vertex, normal));
  glVertexArrayAttribBinding(mesh.vao, attribIndex, bindingIndex);
  glEnableVertexArrayAttrib(mesh.vao, attribIndex);
  attribIndex++;
  glVertexArrayAttribFormat(mesh.vao, attribIndex, 2, GL_FLOAT, GL_FALSE, offsetof(Vertex, texture));
  glVertexArrayAttribBinding(mesh.vao, attribIndex, bindingIndex);
  glEnableVertexArrayAttrib(mesh.vao, attribIndex);
  attribIndex++;
  glVertexArrayAttribFormat(mesh.vao, attribIndex, 3, GL_FLOAT, GL_FALSE, offsetof(Vertex, tangent));
  glVertexArrayAttribBinding(mesh.vao, attribIndex, bindingIndex);
  glEnableVertexArrayAttrib(mesh.vao, attribIndex);
  if (skinned) {
    attribIndex = 4;
    glVertexArrayAttribIFormat(mesh.vao, attribIndex, 4, GL_UNSIGNED_INT, offsetof(Vertex, boneIds));
    glVertexArrayAttribBinding(mesh.vao, attribIndex, bindingIndex);
    glEnableVertexArrayAttrib(mesh.vao, attribIndex);
    attribIndex++;
    glVertexArrayAttribFormat(mesh.vao, attribIndex, 4, GL_FLOAT, GL_FALSE, offsetof(Vertex, boneWeights));
    glVertexArrayAttribBinding(mesh.vao, attribIndex, bindingIndex);
    glEnableVertexArrayAttrib(mesh.vao, attribIndex);
  }
}
auto GLRenderer::ConvertCubemapToEquirectangularMap(const TargetDescription &desc, const unsigned int input) -> unsigned int {
  auto compute = GetCompute("CubemapEquirect");
  if (!compute)
    return 0;
  const auto output = BorrowTexture(desc);
  constexpr unsigned int workgroupSize = 8;
  compute->Use();
  compute->SetTexture("u_cubemap", input);
  compute->SetUniform("u_size", static_cast<unsigned int>(desc.width));
  const auto format = TargetFormatToGL(desc.format);
  glBindImageTexture(0, output, 0, GL_TRUE, 0, GL_WRITE_ONLY, format.internal);
  const auto numGroupsX = static_cast<unsigned int>(std::ceil(static_cast<float>(desc.width) / workgroupSize));
  const auto numGroupsY = static_cast<unsigned int>(std::ceil(static_cast<float>(desc.height) / workgroupSize));
  compute->Dispatch(numGroupsX, numGroupsY, 6);
  return output;
}
auto GLRenderer::ConvertEquirectangularMapToCubemap(const TargetDescription &desc, const unsigned int input, bool invert) -> unsigned int {
  auto compute = GetCompute("EquirectCubemap");
  if (!compute)
    return 0;
  const auto output = BorrowTexture(desc);
  const auto format = TargetFormatToGL(desc.format);
  constexpr unsigned int workgroupSize = 8;
  compute->Use();
  compute->SetTexture("u_equirect", input);
  compute->SetUniform("u_size", static_cast<unsigned int>(desc.width));
  compute->SetUniform("u_invert", invert);
  glBindImageTexture(0, output, 0, GL_TRUE, 0, GL_WRITE_ONLY, format.internal);
  const auto numGroupsX = static_cast<unsigned int>(std::ceil(static_cast<float>(desc.width) / workgroupSize));
  const auto numGroupsY = static_cast<unsigned int>(std::ceil(static_cast<float>(desc.height) / workgroupSize));
  compute->Dispatch(numGroupsX, numGroupsY, 6);
  if (desc.mipmaps > 1) {
    glMemoryBarrier(GL_TEXTURE_UPDATE_BARRIER_BIT);
    glBindTexture(GL_TEXTURE_CUBE_MAP, output);
    glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
  }
  return output;
}
auto GLRenderer::CreateBRDF_LUT(const TargetDescription &desc) -> unsigned int {
  auto compute = GetCompute("BRDF_LUT");
  if (!compute)
    return 0;
  if (const auto output = GetTexture("BRDF_LUT"); output)
    return output->id;
  const auto id = CreateTexture(desc, "BRDF_LUT");
  const auto output = GetTexture(id);
  const auto format = TargetFormatToGL(desc.format);
  constexpr unsigned int workgroupSize = 8;
  compute->Use();
  glBindImageTexture(0, output->id, 0, GL_FALSE, 0, GL_WRITE_ONLY, format.internal);
  const auto numGroupsX = static_cast<unsigned int>(std::ceil(static_cast<float>(desc.width) / workgroupSize));
  const auto numGroupsY = static_cast<unsigned int>(std::ceil(static_cast<float>(desc.height) / workgroupSize));
  compute->Dispatch(numGroupsX, numGroupsY, 1);
  return output->id;
}
auto GLRenderer::CreateIrradianceMap(const TargetDescription &desc, const unsigned int input, const unsigned int inputSize) -> unsigned int {
  auto shProject = GetCompute("SHProject");
  auto irradiance = GetCompute("IrradianceMap");
  if (!shProject || !irradiance)
    return 0;
  const auto output = BorrowTexture(desc);
  const auto format = TargetFormatToGL(desc.format);
  constexpr unsigned int workgroupSize = 8;
  constexpr unsigned int shFaceSize = 32;
  unsigned int shBuffer;
  glCreateBuffers(1, &shBuffer);
  glNamedBufferData(shBuffer, 9 * sizeof(glm::vec4), nullptr, GL_DYNAMIC_DRAW);
  shProject->Use();
  shProject->SetTexture("u_cubemap", input);
  shProject->SetUniform("u_faceSize", shFaceSize);
  shProject->SetUniform("u_mipLevel", std::log2(static_cast<float>(inputSize) / static_cast<float>(shFaceSize)));
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, shBuffer);
  shProject->Dispatch(1, 1, 1);
  glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
  irradiance->Use();
  irradiance->SetUniform("u_cubeSize", static_cast<unsigned int>(desc.width));
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, shBuffer);
  glBindImageTexture(0, output, 0, GL_TRUE, 0, GL_WRITE_ONLY, format.internal);
  const auto numGroupsX = static_cast<unsigned int>(std::ceil(static_cast<float>(desc.width) / workgroupSize));
  const auto numGroupsY = static_cast<unsigned int>(std::ceil(static_cast<float>(desc.height) / workgroupSize));
  irradiance->Dispatch(numGroupsX, numGroupsY, 6);
  glDeleteBuffers(1, &shBuffer);
  return output;
}
auto GLRenderer::CreatePrefilterMap(const TargetDescription &desc, const unsigned int input) -> unsigned int {
  auto compute = GetCompute("PrefilterMap");
  if (!compute)
    return 0;
  const auto output = BorrowTexture(desc);
  const auto format = TargetFormatToGL(desc.format);
  constexpr unsigned int workgroupSize = 8;
  compute->Use();
  compute->SetTexture("u_cubemap", input);
  compute->SetUniform("u_mipLevels", static_cast<unsigned int>(desc.mipmaps));
  for (auto mip = 0; mip < desc.mipmaps; ++mip) {
    const auto mipSize = static_cast<unsigned int>(desc.width) >> mip;
    const auto roughness = static_cast<float>(mip) / (desc.mipmaps - 1);
    compute->SetUniform("u_roughness", roughness);
    compute->SetUniform("u_mipWidth", mipSize);
    compute->SetUniform("u_cubeSize", mipSize);
    glBindImageTexture(0, output, mip, GL_TRUE, 0, GL_WRITE_ONLY, format.internal);
    const auto numGroupsX = static_cast<unsigned int>(std::ceil(static_cast<float>(desc.width) / workgroupSize));
    const auto numGroupsY = static_cast<unsigned int>(std::ceil(static_cast<float>(desc.height) / workgroupSize));
    compute->Dispatch(numGroupsX, numGroupsY, 6);
  }
  return output;
}
auto GLRenderer::LoadMesh(Mesh &mesh, bool skinned) -> GLMesh {
  GLMesh glMesh;
  glMesh.skinned = skinned;
  CreateVertexBuffer(glMesh, mesh.vertices, skinned);
  if (mesh.indices.size() > 0)
    CreateIndexBuffer(glMesh, mesh.indices);
  mesh.vertices = {};
  mesh.indices = {};
  return glMesh;
}
auto GLRenderer::LoadTexture(Texture &texture) -> GLTexture {
  GLTexture glTexture;
  glTexture.desc.width = texture.width;
  glTexture.desc.height = texture.height;
  glTexture.content = texture.content;
  glTexture.flipY = texture.flipY;
  const auto isHDR = texture.range == ColorRange::HDR;
  const auto isSRGB = texture.color == ColorSpace::sRGB;
  if (texture.compression != TextureCompression::None)
    return LoadCompressedTexture(texture, glTexture);
  GLenum internalFormat, format;
  switch (texture.channels) {
  case 1:
    internalFormat = GL_R8;
    format = GL_RED;
    break;
  case 2:
    internalFormat = GL_RG8;
    format = GL_RG;
    break;
  case 3:
    if (isHDR)
      internalFormat = GL_RGB16F;
    else if (isSRGB)
      internalFormat = GL_SRGB8;
    else
      internalFormat = GL_RGB8;
    format = GL_RGB;
    break;
  case 4:
    if (isHDR)
      internalFormat = GL_RGBA16F;
    else if (isSRGB)
      internalFormat = GL_SRGB8_ALPHA8;
    else
      internalFormat = GL_RGBA8;
    format = GL_RGBA;
    break;
  default:
    spdlog::warn("[GLRenderer] Unsupported number of channels: {}", texture.channels);
    return glTexture;
  }
  glTexture.desc.format = GLFormatToTarget(internalFormat);
  glCreateTextures(GL_TEXTURE_2D, 1, &glTexture.id);
  const auto wantsMipmaps = texture.content != TextureContent::Skybox;
  const auto mipmaps = wantsMipmaps ? static_cast<int>(std::log2(std::max(texture.width, texture.height))) + 1 : 1;
  glTextureStorage2D(glTexture.id, mipmaps, internalFormat, texture.width, texture.height);
  const auto type = isHDR ? GL_FLOAT : GL_UNSIGNED_BYTE;
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
  glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
  glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
  std::visit([&](auto &data) { glTextureSubImage2D(glTexture.id, 0, 0, 0, texture.width, texture.height, format, type, data.data()); }, texture.data);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
  switch (texture.content) {
  case TextureContent::Skybox:
    glTextureParameteri(glTexture.id, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(glTexture.id, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTextureParameteri(glTexture.id, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    break;
  default:
    glTextureParameteri(glTexture.id, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTextureParameteri(glTexture.id, GL_TEXTURE_WRAP_T, GL_REPEAT);
    if (texture.channels == 1) {
      glTextureParameteri(glTexture.id, GL_TEXTURE_SWIZZLE_G, GL_RED);
      glTextureParameteri(glTexture.id, GL_TEXTURE_SWIZZLE_B, GL_RED);
      glTextureParameteri(glTexture.id, GL_TEXTURE_SWIZZLE_A, GL_ONE);
    }
    glTextureParameteri(glTexture.id, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glGenerateTextureMipmap(glTexture.id);
    break;
  }
  ReleaseTexturePixels(texture);
  return glTexture;
}
/// @brief Uploads an already block-compressed mip chain, level by level.
///
/// Nothing is generated here. `glGenerateTextureMipmap` cannot produce compressed levels, so the
/// chain `PrepareTexturePixels` built is the whole of what this texture will ever have, and the
/// storage is allocated to exactly that many levels rather than to a full chain that would leave
/// the tail undefined.
auto GLRenderer::LoadCompressedTexture(Texture &texture, GLTexture &glTexture) -> GLTexture {
  const auto *blocks = std::get_if<std::vector<unsigned char>>(&texture.data);
  const auto levels = GetTextureLevels(texture);
  if (!blocks || levels.empty()) {
    spdlog::warn("[GLRenderer] Compressed texture has no data to upload");
    return glTexture;
  }
  const auto isSRGB = texture.color == ColorSpace::sRGB;
  GLenum internalFormat;
  switch (texture.compression) {
  case TextureCompression::BC1:
    internalFormat = isSRGB ? GL_COMPRESSED_SRGB_S3TC_DXT1_EXT : GL_COMPRESSED_RGB_S3TC_DXT1_EXT;
    break;
  case TextureCompression::BC3:
    internalFormat = isSRGB ? GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT : GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
    break;
  case TextureCompression::BC4:
    internalFormat = GL_COMPRESSED_RED_RGTC1;
    break;
  case TextureCompression::BC5:
    internalFormat = GL_COMPRESSED_RG_RGTC2;
    break;
  default:
    spdlog::warn("[GLRenderer] Unsupported texture compression");
    return glTexture;
  }
  glTexture.desc.format = TargetFormat::RGBA8;
  glCreateTextures(GL_TEXTURE_2D, 1, &glTexture.id);
  glTextureStorage2D(glTexture.id, static_cast<int>(levels.size()), internalFormat, texture.width, texture.height);
  for (size_t level = 0; level < levels.size(); ++level) {
    const auto &entry = levels[level];
    if (entry.offset + entry.bytes > blocks->size())
      break;
    glCompressedTextureSubImage2D(glTexture.id, static_cast<int>(level), 0, 0, entry.width, entry.height, internalFormat, static_cast<int>(entry.bytes), blocks->data() + entry.offset);
  }
  glTextureParameteri(glTexture.id, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTextureParameteri(glTexture.id, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glTextureParameteri(glTexture.id, GL_TEXTURE_MIN_FILTER, levels.size() > 1 ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
  glTextureParameteri(glTexture.id, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  if (texture.compression == TextureCompression::BC4) {
    glTextureParameteri(glTexture.id, GL_TEXTURE_SWIZZLE_G, GL_RED);
    glTextureParameteri(glTexture.id, GL_TEXTURE_SWIZZLE_B, GL_RED);
    glTextureParameteri(glTexture.id, GL_TEXTURE_SWIZZLE_A, GL_ONE);
  }
  ReleaseTexturePixels(texture);
  return glTexture;
}
auto GLRenderer::LoadModelMaterial(ModelAsset &modelAsset, const size_t materialIndex) -> EntityID {
  if (materialIndex >= modelAsset.materials.size())
    return EntityID::Invalid;
  auto &modelMaterial = modelAsset.materials[materialIndex];
  if (modelMaterial.resourceId)
    return modelMaterial.resourceId;
  const auto resourceId = resourceRegistry.Create(modelMaterial.name);
  modelMaterial.resourceId = resourceId;
  auto material = resourceRegistry.AddComponent<GLMaterial>(resourceId);
  material->fallback = modelMaterial.fallback;
  material->type = modelMaterial.type;
  for (auto i = 0; i < modelMaterial.textures.size(); ++i) {
    const auto textureIndex = modelMaterial.textures[i];
    const auto texture = LoadModelTexture(modelAsset, textureIndex);
    if (!texture)
      continue;
    switch (texture->content) {
    case TextureContent::Emissive:
      material->textures.emissive = texture->id;
      material->fallback.textureMask.set(static_cast<int>(TextureContent::Emissive));
      break;
    case TextureContent::Metalness:
      material->textures.metalness = texture->id;
      material->fallback.textureMask.set(static_cast<int>(TextureContent::Metalness));
      break;
    case TextureContent::Normal:
      material->textures.normal = texture->id;
      material->fallback.textureMask.set(static_cast<int>(TextureContent::Normal));
      break;
    case TextureContent::Occlusion:
      material->textures.occlusion = texture->id;
      material->fallback.textureMask.set(static_cast<int>(TextureContent::Occlusion));
      break;
    case TextureContent::Roughness:
      material->textures.roughness = texture->id;
      material->fallback.textureMask.set(static_cast<int>(TextureContent::Roughness));
      break;
    case TextureContent::Specular:
      material->textures.specular = texture->id;
      material->fallback.textureMask.set(static_cast<int>(TextureContent::Specular));
      break;
    default:
      material->textures.albedo = texture->id;
      material->fallback.textureMask.set(static_cast<int>(TextureContent::Albedo));
      break;
    }
  }
  spdlog::info("[GLRenderer] Loaded material: {}", modelMaterial.name);
  return resourceId;
}
auto GLRenderer::LoadModelMesh(ModelAsset &modelAsset, const size_t meshIndex) -> EntityID {
  if (meshIndex >= modelAsset.meshes.size())
    return EntityID::Invalid;
  auto &modelMesh = modelAsset.meshes[meshIndex];
  if (modelMesh.resourceId)
    return modelMesh.resourceId;
  const auto resourceId = resourceRegistry.Create(modelMesh.name);
  modelMesh.resourceId = resourceId;
  auto glMesh = resourceRegistry.AddComponent<GLMesh>(resourceId);
  *glMesh = LoadMesh(modelMesh.mesh, !modelMesh.bones.empty());
  const auto materialIndex = modelMesh.material;
  LoadModelMaterial(modelAsset, materialIndex);
  spdlog::info("[GLRenderer] Loaded mesh: {}", modelMesh.name);
  return resourceId;
}
auto GLRenderer::LoadModelTexture(ModelAsset &modelAsset, const size_t textureIndex) -> GLTexture * {
  if (textureIndex >= modelAsset.textures.size())
    return nullptr;
  auto &modelTexture = modelAsset.textures[textureIndex];
  if (modelTexture.resourceId)
    return resourceRegistry.GetComponent<GLTexture>(modelTexture.resourceId);
  const auto resourceId = resourceRegistry.Create(modelTexture.name);
  modelTexture.resourceId = resourceId;
  auto glTexture = resourceRegistry.AddComponent<GLTexture>(resourceId);
  *glTexture = LoadTexture(modelTexture.texture);
  spdlog::info("[GLRenderer] Loaded texture: {}", modelTexture.name);
  return glTexture;
}
auto GLRenderer::UpdatePlaceholderScale(Scene &scene, const EntityID id, const bool pending) -> void {
  const auto wasScaled = placeholderScaledMeshes.contains(id);
  if (pending == wasScaled)
    return;
  auto [bounds, transform] = scene.GetEntityComponent<BoundingBox, Transform>(id);
  if (!bounds || !transform)
    return;
  const auto extents = bounds->max - bounds->min;
  if (extents.x <= 0.f || extents.y <= 0.f || extents.z <= 0.f)
    return;
  if (pending) {
    transform->scale *= extents;
    placeholderScaledMeshes.insert(id);
  } else {
    transform->scale /= extents;
    placeholderScaledMeshes.erase(id);
  }
}
} // namespace kuki
