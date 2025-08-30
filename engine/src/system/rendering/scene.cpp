#define GLM_ENABLE_EXPERIMENTAL
#include <system/rendering.hpp>
#include <application.hpp>
#include <component/camera.hpp>
#include <component/component.hpp>
#include <component/light.hpp>
#include <component/material.hpp>
#include <component/mesh.hpp>
#include <component/mesh_filter.hpp>
#include <component/mesh_renderer.hpp>
#include <component/shader.hpp>
#include <component/transform.hpp>
#include <component/skybox.hpp>
#include <glm/detail/qualifier.hpp>
#include <glm/ext/matrix_float3x3.hpp>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/gtx/quaternion.hpp>
#include <spdlog/spdlog.h>
#include <unordered_map>
#include <vector>
namespace kuki {
int RenderingSystem::RenderSceneToTexture(Camera* camera) {
  if (!camera)
    return -1;
  auto& config = app.GetConfig();
  auto width = config.screenWidth;
  auto height = config.screenHeight;
  // FIXME: this prevents users from experimenting with different aspect ratios in the editor
  camera->aspectRatio = static_cast<float>(width) / height;
  const TextureParams singleParams{width, height, GL_TEXTURE_2D, GL_RGB16F, 1, 1};
  const TextureParams multiParams{width, height, GL_TEXTURE_2D_MULTISAMPLE, GL_RGB16F, 4, 1};
  auto framebufferMulti = framebufferPool.Request(multiParams);
  auto framebufferSingle = framebufferPool.Request(singleParams);
  auto renderbufferMulti = renderbufferPool.Request(multiParams);
  auto renderbufferSingle = renderbufferPool.Request(singleParams);
  auto textureMulti = texturePool.Request(multiParams);
  auto textureSingle = texturePool.Request(singleParams);
  UpdateAttachments(multiParams, framebufferMulti, renderbufferMulti, textureMulti);
  UpdateAttachments(singleParams, framebufferSingle, renderbufferSingle, textureSingle);
  glBindFramebuffer(GL_FRAMEBUFFER, framebufferMulti);
  glViewport(0, 0, width, height);
  glClearColor(.0f, .0f, .0f, .0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
  auto sceneCamId = app.GetEntityId("SceneCamera");
  auto sceneCam = app.GetEntityComponent<Camera>(sceneCamId);
  // HACK: we can't get the scene camera by calling GetActiveCamera() because editor camera is also in the scene
  // TODO: hide editor camera from scene hierarchy
  DrawScene(sceneCam ? sceneCam : camera, camera);
  DrawGizmos(sceneCam ? sceneCam : camera, camera);
  glBindFramebuffer(GL_READ_FRAMEBUFFER, framebufferMulti);
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, framebufferSingle);
  glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_LINEAR);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  auto texturePost = texturePool.Request(singleParams);
  ApplyPostProc(textureSingle, texturePost, singleParams);
  framebufferPool.Release(multiParams, framebufferMulti);
  framebufferPool.Release(singleParams, framebufferSingle);
  renderbufferPool.Release(multiParams, renderbufferMulti);
  renderbufferPool.Release(singleParams, renderbufferSingle);
  texturePool.Release(multiParams, textureMulti);
  texturePool.Release(singleParams, texturePost);
  texturePool.Release(singleParams, textureSingle);
  return texturePost;
}
void RenderingSystem::ApplyPostProc(unsigned int textureIn, unsigned int textureOut, const TextureParams& params) {
  auto bloomShader = GetShader(MaterialType::Bloom);
  if (!bloomShader) {
    spdlog::warn("Shader not found: {}.", EnumTraits<MaterialType>().GetNames().at(static_cast<uint8_t>(MaterialType::Bloom)));
    return;
  }
  auto brightShader = GetShader(MaterialType::Bright);
  if (!brightShader) {
    spdlog::warn("Shader not found: {}.", EnumTraits<MaterialType>().GetNames().at(static_cast<uint8_t>(MaterialType::Bright)));
    return;
  }
  auto blurShader = GetShader(MaterialType::Blur);
  if (!blurShader) {
    spdlog::warn("Shader not found: {}.", EnumTraits<MaterialType>().GetNames().at(static_cast<uint8_t>(MaterialType::Blur)));
    return;
  }
  auto frameId = app.GetAssetId("Frame");
  auto mesh = app.GetAssetComponent<Mesh>(frameId);
  if (!mesh) {
    spdlog::warn("Frame mesh not found.");
    return;
  }
  auto framebufferPing = framebufferPool.Request(params);
  auto framebufferPong = framebufferPool.Request(params);
  auto renderbufferPing = renderbufferPool.Request(params);
  auto renderbufferPong = renderbufferPool.Request(params);
  auto texturePing = texturePool.Request(params);
  auto texturePong = texturePool.Request(params);
  auto textureNormal = texturePool.Request(params);
  UpdateAttachments(params, framebufferPong, renderbufferPong, textureNormal, texturePong);
  glBindFramebuffer(GL_FRAMEBUFFER, framebufferPong);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, textureIn);
  brightShader->Use();
  brightShader->SetUniform("image", 0);
  glViewport(0, 0, params.width, params.height);
  glClearColor(.0f, .0f, .0f, .0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  brightShader->SetUniform("model", glm::mat4(1.0f));
  GLuint attachments[2] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
  glDrawBuffers(2, attachments);
  brightShader->Draw(mesh);
  const auto blurPasses = 8;
  blurShader->Use();
  blurShader->SetUniform("model", glm::mat4(1.0f));
  blurShader->SetUniform("image", 0);
  for (auto i = 0; i < blurPasses; ++i) {
    auto pingPong = i % 2 == 0;
    auto framebuffer = pingPong ? framebufferPing : framebufferPong;
    auto renderbuffer = pingPong ? renderbufferPing : renderbufferPong;
    auto textureTarget = pingPong ? texturePing : texturePong;
    auto textureSource = pingPong ? texturePong : texturePing;
    UpdateAttachments(params, framebuffer, renderbuffer, textureTarget);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glBindTexture(GL_TEXTURE_2D, textureSource);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    blurShader->SetUniform("horizontal", pingPong);
    blurShader->Draw(mesh);
  }
  glDrawBuffer(GL_COLOR_ATTACHMENT0);
  UpdateAttachments(params, framebufferPing, renderbufferPing, textureOut);
  glBindFramebuffer(GL_FRAMEBUFFER, framebufferPing);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, textureNormal);
  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, texturePong);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  bloomShader->Use();
  bloomShader->SetUniform("hdrImage", 0);
  bloomShader->SetUniform("bloomImage", 1);
  bloomShader->SetUniform("exposure", 1.0f);
  bloomShader->SetUniform("gamma", 2.2f);
  bloomShader->SetUniform("model", glm::mat4(1.0f));
  bloomShader->Draw(mesh);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  framebufferPool.Release(params, framebufferPing);
  framebufferPool.Release(params, framebufferPong);
  renderbufferPool.Release(params, renderbufferPing);
  renderbufferPool.Release(params, renderbufferPong);
  texturePool.Release(params, texturePing);
  texturePool.Release(params, texturePong);
  texturePool.Release(params, textureNormal);
}
void RenderingSystem::DrawScene(const Camera* camera, const Camera* observer) {
  if (!camera)
    return;
  std::unordered_map<unsigned int, std::vector<unsigned int>> vaoToEntities;
  std::unordered_map<unsigned int, Mesh> vaoToMesh;
  // FIXME: app.ForEachVisibleEntity does not work as expected
  app.ForEachVisibleEntity(*camera, [this, &vaoToMesh, &vaoToEntities](unsigned int id) {
    auto filter = app.GetEntityComponent<MeshFilter>(id);
    auto vao = filter->mesh.vertexArray;
    vaoToMesh[vao] = filter->mesh;
    vaoToEntities[vao].push_back(id);
  });
  if (wireframeMode)
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
  auto targetCam = observer ? observer : camera;
  for (const auto& [vao, entities] : vaoToEntities)
    DrawEntitiesInstanced(targetCam, &vaoToMesh[vao], entities);
  glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
  app.ForFirstEntity<Skybox>([this, &targetCam](unsigned int id, Skybox* skybox) {
    DrawSkybox(targetCam, skybox);
  });
}
void RenderingSystem::DrawSkybox(const Camera* camera, const Skybox* skybox) {
  if (!camera || !skybox || skybox->skybox == 0)
    return;
  auto cubeAsset = app.GetAssetId("CubeInverted");
  auto mesh = app.GetAssetComponent<Mesh>(cubeAsset);
  if (!mesh)
    return;
  auto shader = GetShader(MaterialType::Skybox);
  shader->Use();
  auto model = glm::scale(glm::mat4(1.0f), glm::vec3(2.0f));
  shader->SetUniform("model", model);
  auto view = glm::mat4(glm::mat3(camera->view));
  shader->SetUniform("view", view);
  shader->SetUniform("projection", camera->projection);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_CUBE_MAP, skybox->skybox);
  shader->SetUniform("skybox", 0);
  glDepthFunc(GL_LEQUAL);
  shader->Draw(mesh);
  glDepthFunc(GL_LESS);
}
void RenderingSystem::DrawEntitiesInstanced(const Camera* camera, const Mesh* mesh, const std::vector<unsigned int>& entities) {
  if (!camera || !mesh)
    return;
  std::vector<LitFallbackData> litMaterials;
  std::vector<UnlitFallbackData> unlitMaterials;
  std::vector<glm::mat4> litTransforms;
  std::vector<glm::mat4> unlitTransforms;
  // NOTE: following variables will store the last instance of that material type
  Material materialLit;
  Material materialUnlit;
  // TODO: a separate draw call shall be invoked per unique material configuration (e.g. different albedo textures)
  for (const auto id : entities) {
    auto [transform, renderer] = app.GetEntityComponents<Transform, MeshRenderer>(id);
    if (!renderer)
      continue;
    if (auto litMaterial = std::get_if<LitMaterial>(&materialLit.material)) {
      materialLit = renderer->material;
      litMaterials.push_back(litMaterial->fallback);
      litTransforms.push_back(transform->world);
    } else if (auto unlitMaterial = std::get_if<UnlitMaterial>(&materialUnlit.material)) {
      materialUnlit = renderer->material;
      unlitMaterials.push_back(unlitMaterial->fallback);
      unlitTransforms.push_back(transform->world);
    } // else ...
  }
  if (litMaterials.size() > 0) {
    Skybox* skybox{};
    app.ForFirstEntity<Skybox>([this, &skybox](unsigned int id, Skybox* skyboxComp) {
      skybox = skyboxComp;
    });
    auto shader = static_cast<LitShader*>(GetShader(MaterialType::Lit));
    std::vector<const Light*> lights;
    app.ForEachEntity<Light>([&](unsigned int id, Light* light) {
      lights.push_back(light);
    });
    shader->Use();
    shader->SetCamera(camera);
    shader->SetLighting(lights);
    if (skybox && skybox->skybox != 0) {
      // TODO: let the shader handle which texture unit to use
      glActiveTexture(GL_TEXTURE5); // units 0-4 are used by other textures such as albedo map
      glBindTexture(GL_TEXTURE_CUBE_MAP, skybox->irradiance);
      glActiveTexture(GL_TEXTURE6);
      glBindTexture(GL_TEXTURE_CUBE_MAP, skybox->prefilter);
      glActiveTexture(GL_TEXTURE7);
      glBindTexture(GL_TEXTURE_2D, skybox->brdf);
      shader->SetUniform("irradianceMap", 5);
      shader->SetUniform("prefilterMap", 6);
      shader->SetUniform("brdfLUT", 7);
      shader->SetUniform("hasSkybox", true);
    } else
      shader->SetUniform("hasSkybox", false);
    shader->SetMaterial(&materialLit);
    shader->SetMaterialFallback(mesh, litMaterials, materialVBO);
    shader->SetTransform(mesh, litTransforms, transformVBO);
    shader->DrawInstanced(mesh, litTransforms.size());
  }
  if (unlitMaterials.size() > 0) {
    auto shader = static_cast<UnlitShader*>(GetShader(MaterialType::Unlit));
    shader->Use();
    shader->SetCamera(camera);
    shader->SetMaterial(&materialUnlit);
    shader->SetMaterialFallback(mesh, unlitMaterials, materialVBO);
    shader->SetTransform(mesh, unlitTransforms, transformVBO);
    shader->DrawInstanced(mesh, unlitTransforms.size());
  }
}
} // namespace kuki
