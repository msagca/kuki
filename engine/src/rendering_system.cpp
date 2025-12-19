#define GLM_ENABLE_EXPERIMENTAL
#include <application.hpp>
#include <bounding_box.hpp>
#include <camera.hpp>
#include <cmath>
#include <component.hpp>
#include <cstdint>
#include <deque>
#include <glad/glad.h>
#include <glm/detail/type_vec3.hpp>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/ext/vector_float4.hpp>
#include <id.hpp>
#include <light.hpp>
#include <material.hpp>
#include <mesh.hpp>
#include <mesh_filter.hpp>
#include <mesh_renderer.hpp>
#include <octree.hpp>
#include <pool.hpp>
#include <rendering_system.hpp>
#include <shader.hpp>
#include <skybox.hpp>
#include <spdlog/spdlog.h>
#include <system.hpp>
#include <texture.hpp>
#include <texture_params.hpp>
#include <texture_pool.hpp>
#include <transform.hpp>
#include <unordered_map>
#include <variant>
#include <vector>
namespace kuki {
RenderingSystem::RenderingSystem(Application& app)
  : System(app) {}
RenderingSystem::~RenderingSystem() {
  Shutdown();
}
void RenderingSystem::Start() {
  glCreateBuffers(1, &materialVBO);
  glCreateBuffers(1, &transformVBO);
  auto brdfCompute = new ComputeShader("BRDF_LUT", "shader/brdf_lut.comp", *this, ComputeType::BRDF_LUT);
  auto cubeMapEquirectCompute = new ComputeShader("CubeMapEquirect", "shader/cubemap_equirect.comp", *this, ComputeType::CubeMapEquirect);
  auto equirectCubeMapCompute = new ComputeShader("EquirectCubeMap", "shader/equirect_cubemap.comp", *this, ComputeType::EquirectCubeMap);
  auto irradianceCompute = new ComputeShader("Irradiance", "shader/irradiance.comp", *this, ComputeType::IrradianceMap);
  auto prefilterCompute = new ComputeShader("Prefilter", "shader/prefilter.comp", *this, ComputeType::PrefilterMap);
  auto bloomShader = new Shader("Postprocessing", "shader/standard_m.vert", "shader/bloom.frag", *this, MaterialType::Bloom);
  auto blurShader = new Shader("Blur", "shader/standard_m.vert", "shader/blur.frag", *this, MaterialType::Blur);
  auto brightShader = new Shader("BrightPass", "shader/standard_m.vert", "shader/bright_pass.frag", *this, MaterialType::BrightPass);
  auto litShader = new LitShader("Lit", "shader/lit.vert", "shader/lit.frag", *this);
  auto litSkinnedShader = new LitShader("LitSkinned", "shader/lit_skinned.vert", "shader/lit.frag", *this);
  auto skyboxShader = new Shader("Skybox", "shader/skybox.vert", "shader/skybox.frag", *this, MaterialType::Skybox);
  auto unlitShader = new UnlitShader("Unlit", "shader/unlit.vert", "shader/unlit.frag", *this);
  computes.insert({brdfCompute->GetType(), brdfCompute});
  computes.insert({cubeMapEquirectCompute->GetType(), cubeMapEquirectCompute});
  computes.insert({equirectCubeMapCompute->GetType(), equirectCubeMapCompute});
  computes.insert({irradianceCompute->GetType(), irradianceCompute});
  computes.insert({prefilterCompute->GetType(), prefilterCompute});
  shaders.insert({bloomShader->GetType(), bloomShader});
  shaders.insert({blurShader->GetType(), blurShader});
  shaders.insert({brightShader->GetType(), brightShader});
  shaders.insert({litShader->GetType(), litShader});
  shaders.insert({litSkinnedShader->GetType(), litSkinnedShader});
  shaders.insert({skyboxShader->GetType(), skyboxShader});
  shaders.insert({unlitShader->GetType(), unlitShader});
}
void RenderingSystem::Update(float deltaTime) {
  static std::deque<float> times;
  static auto accumulatedTime = 0.f;
  times.push_back(deltaTime);
  accumulatedTime += deltaTime;
  while (accumulatedTime > 1.f && !times.empty()) {
    auto lastTime = times.front();
    accumulatedTime -= lastTime;
    times.pop_front();
  }
  fps = times.size();
}
void RenderingSystem::LateUpdate(float deltaTime) {
  UpdateEntityTransforms();
  UpdateCameraTransforms();
}
void RenderingSystem::Shutdown() {
  glDeleteVertexArrays(1, &materialVBO);
  glDeleteVertexArrays(1, &transformVBO);
  for (const auto& [_, compute] : computes)
    delete compute;
  for (const auto& [_, shader] : shaders)
    delete shader;
  for (auto& [_, texture] : assetToTexture) {
    glDeleteTextures(1, &texture);
    texture = 0;
  }
  glDeleteTextures(1, &brdf.id);
  brdf.id = 0;
  computes.clear();
  shaders.clear();
  framebufferPool.Clear();
  renderbufferPool.Clear();
  texturePool.Clear();
  uniformBufferPool.Clear();
  // FIXME: make sure entities return GPU resources at destruction
}
size_t RenderingSystem::GetFPS() const {
  return fps;
}
bool RenderingSystem::wireframeMode = false;
void RenderingSystem::ToggleWireframeMode() {
  wireframeMode = !wireframeMode;
}
Shader* RenderingSystem::GetShader(MaterialType type) {
  switch (type) {
  case MaterialType::Lit:
    return shaders[MaterialType::Lit];
  case MaterialType::Unlit:
    return shaders[MaterialType::Unlit];
  case MaterialType::Skybox:
    return shaders[MaterialType::Skybox];
  case MaterialType::Bloom:
    return shaders[MaterialType::Bloom];
  case MaterialType::BrightPass:
    return shaders[MaterialType::BrightPass];
  case MaterialType::Blur:
    return shaders[MaterialType::Blur];
  default:
    return nullptr;
  }
}
ComputeShader* RenderingSystem::GetCompute(ComputeType type) {
  switch (type) {
  case ComputeType::BRDF_LUT:
    return computes[ComputeType::BRDF_LUT];
  case ComputeType::CubeMapEquirect:
    return computes[ComputeType::CubeMapEquirect];
  case ComputeType::EquirectCubeMap:
    return computes[ComputeType::EquirectCubeMap];
  case ComputeType::IrradianceMap:
    return computes[ComputeType::IrradianceMap];
  case ComputeType::PrefilterMap:
    return computes[ComputeType::PrefilterMap];
  default:
    return nullptr;
  }
}
void RenderingSystem::UpdateEntityTransforms() {
  app.UpdateEntityTransforms();
  // auto scene = app.GetActiveScene();
  // if (!scene)
  //   return;
  // FIXME: do not traverse every Transform every frame
  // app.ForEachEntity<Transform>([this, &scene](EntityID id, Transform* transform) {
  //   auto filter = app.GetEntityComponent<MeshFilter>(id);
  //   if (!filter)
  //     return;
  //   scene->octree.Insert(id, filter->mesh.bounds.GetWorldBounds(transform->world));
  // });
}
void RenderingSystem::UpdateCameraTransforms() {
  app.ForEachEntity<Camera>([](ID id, Camera* camera) {
    if (camera->positionDirty || camera->rotationDirty || camera->settingsDirty)
      camera->Update();
  });
}
int RenderingSystem::RenderSceneToTexture(Camera* camera) {
  if (!camera)
    return -1;
  auto& config = app.GetConfig();
  const auto width = config.screenWidth;
  const auto height = config.screenHeight;
  // FIXME: the following prevents users from experimenting with different aspect ratios in the editor
  const auto aspect = static_cast<float>(width) / height;
  if (camera->aspectRatio != aspect) {
    camera->aspectRatio = aspect;
    camera->rotationDirty = true;
    camera->uboDirty = true;
  }
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
  glClearColor(0.f, 0.f, 0.f, 0.f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
  DrawScene(camera, camera);
  DrawGizmos(camera, camera);
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
  auto brightShader = GetShader(MaterialType::BrightPass);
  if (!brightShader) {
    spdlog::warn("Shader not found: {}.", EnumTraits<MaterialType>().GetNames().at(static_cast<uint8_t>(MaterialType::BrightPass)));
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
  glClearColor(0.f, 0.f, 0.f, 0.f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  brightShader->SetUniform("model", glm::mat4(1.f));
  unsigned int attachments[2] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
  glDrawBuffers(2, attachments);
  brightShader->Draw(mesh);
  const auto blurPasses = 8;
  blurShader->Use();
  blurShader->SetUniform("model", glm::mat4(1.f));
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
  bloomShader->SetUniform("exposure", 1.f);
  bloomShader->SetUniform("gamma", 2.2f);
  bloomShader->SetUniform("model", glm::mat4(1.f));
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
int RenderingSystem::RenderAssetToTexture(ID assetId, const int textureSize) {
  const auto [texture, skybox] = app.GetAssetComponents<Texture, Skybox>(assetId);
  GLuint textureIdPost = 0;
  if (auto it = assetToTexture.find(assetId); it != assetToTexture.end())
    textureIdPost = it->second;
  else if (texture)
    textureIdPost = texture->id;
  else if (skybox)
    textureIdPost = skybox->preview;
  else {
    TextureParams textureParams{textureSize, textureSize, GL_TEXTURE_2D, GL_RGBA32F};
    auto textureIdPre = texturePool.Request(textureParams);
    textureIdPost = texturePool.Request(textureParams);
    auto framebuffer = framebufferPool.Request(textureParams);
    auto renderbuffer = renderbufferPool.Request(textureParams);
    UpdateAttachments(textureParams, framebuffer, renderbuffer, textureIdPre);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glViewport(0, 0, textureSize, textureSize);
    glClearColor(0.f, 0.f, 0.f, 0.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    DrawAssetHierarchy(assetId);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    framebufferPool.Release(textureParams, framebuffer);
    renderbufferPool.Release(textureParams, renderbuffer);
    ApplyPostProc(textureIdPre, textureIdPost, textureParams);
    texturePool.Release(textureParams, textureIdPre);
    assetToTexture[assetId] = textureIdPost;
  }
  return textureIdPost;
}
void RenderingSystem::DrawScene(const Camera* camera, const Camera* observer) {
  if (!camera)
    return;
  std::unordered_map<unsigned int, std::vector<ID>> vaoToEntities;
  std::unordered_map<unsigned int, Mesh> vaoToMesh;
  // TODO: use ForEachVisibleEntity below
  app.ForEachEntity<MeshFilter>([this, &vaoToMesh, &vaoToEntities](ID id, MeshFilter* filter) {
    auto vao = filter->mesh.vao;
    vaoToMesh[vao] = filter->mesh;
    vaoToEntities[vao].push_back(id);
  });
  if (wireframeMode)
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
  auto targetCam = observer ? observer : camera;
  for (const auto& [vao, entities] : vaoToEntities)
    DrawEntitiesInstanced(targetCam, &vaoToMesh[vao], entities);
  if (wireframeMode)
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
  app.ForFirstEntity<Skybox>([this, &targetCam](ID id, Skybox* skybox) {
    DrawSkybox(targetCam, skybox);
  });
}
void RenderingSystem::DrawSkybox(const Camera* camera, const Skybox* skybox) {
  if (!camera || !skybox)
    return;
  auto cubeAsset = app.GetAssetId("CubeInverted");
  auto mesh = app.GetAssetComponent<Mesh>(cubeAsset);
  if (!mesh)
    return;
  auto shader = GetShader(MaterialType::Skybox);
  shader->Use();
  shader->SetCamera(camera);
  auto model = glm::scale(glm::mat4(1.f), glm::vec3(2.f));
  shader->SetUniform("model", model);
  if (skybox->original == 0)
    shader->SetUniform("useSkybox", false);
  else {
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, skybox->original);
    shader->SetUniform("skybox", 0);
    shader->SetUniform("useSkybox", true);
  }
  glDepthFunc(GL_LEQUAL);
  shader->Draw(mesh);
  glDepthFunc(GL_LESS);
}
void RenderingSystem::DrawEntitiesInstanced(const Camera* camera, const Mesh* mesh, const std::vector<ID>& entities) {
  if (!camera || !mesh)
    return;
  std::vector<LitFallbackData> litMaterials;
  std::vector<UnlitFallbackData> unlitMaterials;
  std::vector<glm::mat4> litTransforms;
  std::vector<glm::mat4> unlitTransforms;
  // NOTE: following variables will store the last instance of that material type
  Material materialLit;
  Material materialUnlit;
  // FIXME: a separate draw call shall be invoked per unique material configuration (e.g., different albedo textures)
  for (auto id : entities) {
    auto [renderer, transform] = app.GetEntityComponents<MeshRenderer, Transform>(id);
    if (!renderer || !transform)
      continue;
    if (auto litMaterial = std::get_if<LitMaterial>(&renderer->material.current)) {
      materialLit = renderer->material;
      litMaterials.push_back(litMaterial->fallback);
      litTransforms.push_back(transform->world);
    } else if (auto unlitMaterial = std::get_if<UnlitMaterial>(&renderer->material.current)) {
      materialUnlit = renderer->material;
      unlitMaterials.push_back(unlitMaterial->fallback);
      unlitTransforms.push_back(transform->world);
    } // else ...
  }
  if (litMaterials.size() > 0) {
    Skybox* skybox{nullptr};
    app.ForFirstEntity<Skybox>([this, &skybox](ID id, Skybox* skyboxComp) {
      skybox = skyboxComp;
    });
    auto shader = static_cast<LitShader*>(GetShader(MaterialType::Lit));
    std::vector<const Light*> lights;
    app.ForEachEntity<Light>([&](ID id, Light* light) {
      lights.push_back(light);
    });
    shader->Use();
    shader->SetCamera(camera);
    shader->SetLighting(lights);
    if (skybox) {
      // TODO: let the shader handle which texture units to use
      // NOTE: units 0-6 are used by other textures such as albedo map
      if (skybox->irradiance > 0) {
        glActiveTexture(GL_TEXTURE7);
        glBindTexture(GL_TEXTURE_CUBE_MAP, skybox->irradiance);
        shader->SetUniform("irradianceMap", 7);
      }
      if (skybox->prefilter > 0) {
        glActiveTexture(GL_TEXTURE8);
        glBindTexture(GL_TEXTURE_CUBE_MAP, skybox->prefilter);
        shader->SetUniform("prefilterMap", 8);
      }
      if (skybox->brdf > 0) {
        glActiveTexture(GL_TEXTURE9);
        glBindTexture(GL_TEXTURE_2D, skybox->brdf);
        shader->SetUniform("brdfLUT", 9);
      }
      shader->SetUniform("hasSkybox", true);
      shader->SetUniform("hasIrradianceMap", skybox->irradiance > 0);
      shader->SetUniform("hasPrefilterMap", skybox->prefilter > 0);
      shader->SetUniform("hasBRDF", skybox->brdf > 0);
    } else {
      shader->SetUniform("hasSkybox", false);
      shader->SetUniform("hasIrradianceMap", false);
      shader->SetUniform("hasPrefilterMap", false);
      shader->SetUniform("hasBRDF", false);
    }
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
void RenderingSystem::DrawAssetHierarchy(ID id) {
  static const Light dirLight{};
  auto shader = static_cast<LitShader*>(GetShader(MaterialType::Lit));
  auto bounds = GetAssetBounds(id);
  assetCam.Frame(bounds);
  shader->Use();
  shader->SetCamera(&assetCam);
  shader->SetLighting(&dirLight);
  shader->SetUniform("hasSkybox", false);
  DrawAsset(id);
}
void RenderingSystem::DrawAsset(ID id) {
  auto [transform, mesh, material] = app.GetAssetComponents<Transform, Mesh, Material>(id);
  if (transform && mesh && material) {
    auto model = transform->world;
    auto shader = static_cast<LitShader*>(GetShader(MaterialType::Lit));
    shader->SetMaterial(material);
    if (auto litMaterial = std::get_if<LitMaterial>(&material->current))
      // TODO: support other materials
      shader->SetMaterialFallback(mesh, litMaterial->fallback, materialVBO);
    shader->SetTransform(mesh, model, transformVBO);
    shader->Draw(mesh);
  }
  app.ForEachChildAsset(id, [this](ID childId) {
    DrawAsset(childId);
  });
}
BoundingBox RenderingSystem::GetAssetBounds(ID id) {
  BoundingBox bounds{};
  auto [mesh, transform] = app.GetAssetComponents<Mesh, Transform>(id);
  if (transform && mesh)
    bounds = mesh->bounds.GetWorldBounds(transform->world);
  app.ForEachChildAsset(id, [&](ID childId) {
    auto childBounds = GetAssetBounds(childId);
    bounds.min = glm::min(bounds.min, childBounds.min);
    bounds.max = glm::max(bounds.max, childBounds.max);
  });
  return bounds;
}
void RenderingSystem::DrawGizmos(const Camera* camera, const Camera* observer) {
  if (!camera || camera == observer)
    return;
  auto manipulatorEnabled = (gizmoMask & static_cast<unsigned int>(GizmoMask::Manipulator)) != 0;
  auto viewFrustumEnabled = (gizmoMask & static_cast<unsigned int>(GizmoMask::ViewFrustum)) != 0;
  auto frustumCullingEnabled = (gizmoMask & static_cast<unsigned int>(GizmoMask::FrustumCulling)) != 0;
  if (viewFrustumEnabled)
    DrawViewFrustum(camera, observer);
  if (frustumCullingEnabled)
    DrawFrustumCulling(camera, observer);
}
void RenderingSystem::DrawFrustumCulling(const Camera* camera, const Camera* observer) {
  auto assetId = app.GetAssetId("Cube");
  auto mesh = app.GetAssetComponent<Mesh>(assetId);
  if (!mesh)
    return;
  glm::vec4 color{};
  color.a = .2f;
  std::vector<glm::mat4> transforms;
  std::vector<UnlitFallbackData> materials;
  app.ForEachOctreeLeafNode([&](OctreeNode<ID>* node, Octant octant) {
    auto depth = node->depth;
    auto maxDepth = node->maxDepth;
    auto center = node->center;
    auto extent = node->extent;
    auto intersects = camera->IntersectsFrustum(node->bounds);
    auto model = glm::mat4(1.f);
    model = glm::translate(model, center);
    model = glm::scale(model, extent * 2.f);
    auto ratio = static_cast<unsigned int>(octant) / 16.f;
    color.r = intersects ? 0.f : .5f + ratio;
    color.g = intersects ? .5f + ratio : 0.f;
    color.b = maxDepth > 0 ? static_cast<float>(depth) / maxDepth : 1.f;
    transforms.push_back(model);
    UnlitFallbackData material{};
    material.base = color;
    materials.push_back(material);
  });
  auto shader = static_cast<UnlitShader*>(GetShader(MaterialType::Unlit));
  shader->Use();
  shader->SetCamera(observer);
  shader->SetMaterialFallback(mesh, materials, materialVBO);
  shader->SetTransform(mesh, transforms, transformVBO);
  shader->DrawInstanced(mesh, transforms.size());
}
void RenderingSystem::DrawViewFrustum(const Camera* camera, const Camera* observer) {
  static Material material;
  auto frameId = app.GetAssetId("Frame");
  auto frameMesh = app.GetAssetComponent<Mesh>(frameId);
  if (!frameMesh)
    return;
  material.current = UnlitMaterial();
  auto& unlitMaterial = std::get<UnlitMaterial>(material.current);
  unlitMaterial.fallback.base = glm::vec4(.5f, .5f, 0.f, .2f);
  auto shader = static_cast<UnlitShader*>(GetShader(MaterialType::Unlit));
  shader->Use();
  shader->SetCamera(observer);
  shader->SetMaterial(&material);
  shader->SetMaterialFallback(frameMesh, unlitMaterial.fallback, materialVBO);
  glDisable(GL_CULL_FACE);
  // near plane (z = -1 in NDC)
  auto nearModel = glm::scale(glm::mat4(1.f), glm::vec3(2.f, 2.f, 1.f));
  nearModel = glm::translate(nearModel, glm::vec3(0.f, 0.f, -1.f));
  nearModel = glm::inverse(camera->transform.projection * camera->transform.view) * nearModel;
  shader->SetTransform(frameMesh, nearModel, transformVBO);
  shader->DrawInstanced(frameMesh, 1);
  // far plane (z = 1 in NDC)
  auto farModel = glm::scale(glm::mat4(1.f), glm::vec3(2.f, 2.f, 1.f));
  farModel = glm::translate(farModel, glm::vec3(0.f, 0.f, 1.f));
  farModel = glm::inverse(camera->transform.projection * camera->transform.view) * farModel;
  shader->SetTransform(frameMesh, farModel, transformVBO);
  shader->DrawInstanced(frameMesh, 1);
  glEnable(GL_CULL_FACE);
}
size_t RenderingSystem::GetGizmoMask() const {
  return gizmoMask;
}
void RenderingSystem::SetGizmoMask(size_t mask) {
  gizmoMask = mask;
}
Texture RenderingSystem::CreateCubeMapFromEquirect(Texture equirect, const int textureSize) {
  constexpr unsigned int workgroupSize = 8;
  Texture texture{};
  texture.type = TextureType::CubeMap;
  texture.width = textureSize;
  texture.height = textureSize;
  const auto numGroups = static_cast<unsigned int>(std::ceil(static_cast<float>(textureSize) / workgroupSize));
  TextureParams params{texture.width, texture.height, GL_TEXTURE_CUBE_MAP, GL_RGBA32F};
  texture.id = texturePool.Request(params);
  auto shader = GetCompute(ComputeType::EquirectCubeMap);
  if (!shader) {
    spdlog::warn("Compute shader not found: {}.", EnumTraits<ComputeType>().GetNames().at(static_cast<uint8_t>(ComputeType::EquirectCubeMap)));
    return texture;
  }
  shader->Use();
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, equirect.id);
  shader->SetUniform("equirect", 0);
  shader->SetUniform("size", static_cast<unsigned int>(textureSize));
  shader->SetUniform("invert", equirect.type == TextureType::EXR);
  glBindImageTexture(0, texture.id, 0, GL_TRUE, 0, GL_WRITE_ONLY, params.format);
  glDispatchCompute(numGroups, numGroups, 6);
  glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
  return texture;
}
Texture RenderingSystem::CreateEquirectFromCubeMap(Texture cubeMap, const int textureSize) {
  constexpr unsigned int workgroupSize = 8;
  Texture texture{};
  texture.type = TextureType::Equirect;
  texture.width = textureSize;
  texture.height = textureSize;
  const auto numGroups = static_cast<unsigned int>(std::ceil(static_cast<float>(textureSize) / workgroupSize));
  TextureParams params{texture.width, texture.height, GL_TEXTURE_2D, GL_RGBA32F};
  texture.id = texturePool.Request(params);
  auto shader = GetCompute(ComputeType::CubeMapEquirect);
  if (!shader) {
    spdlog::warn("Compute shader not found: {}.", EnumTraits<ComputeType>().GetNames().at(static_cast<uint8_t>(ComputeType::CubeMapEquirect)));
    return texture;
  }
  shader->Use();
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_CUBE_MAP, cubeMap.id);
  shader->SetUniform("cubeMap", 0);
  shader->SetUniform("size", static_cast<unsigned int>(textureSize));
  glBindImageTexture(0, texture.id, 0, GL_TRUE, 0, GL_WRITE_ONLY, params.format);
  glDispatchCompute(numGroups, numGroups, 6);
  glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
  return texture;
}
Texture RenderingSystem::CreateIrradianceMapFromCubeMap(Texture cubeMap, const int textureSize) {
  constexpr unsigned int workgroupSize = 8;
  const auto numGroups = static_cast<unsigned int>(std::ceil(static_cast<float>(textureSize) / workgroupSize));
  Texture texture{};
  texture.type = TextureType::Irradiance;
  texture.width = textureSize;
  texture.height = textureSize;
  TextureParams params{textureSize, textureSize, GL_TEXTURE_CUBE_MAP, GL_RGBA32F};
  texture.id = texturePool.Request(params);
  auto shader = GetCompute(ComputeType::IrradianceMap);
  if (!shader) {
    spdlog::warn("Compute shader not found: {}.", EnumTraits<ComputeType>().GetNames().at(static_cast<uint8_t>(ComputeType::IrradianceMap)));
    return texture;
  }
  shader->Use();
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_CUBE_MAP, cubeMap.id);
  shader->SetUniform("cubeMap", 0);
  shader->SetUniform("cubeSize", static_cast<unsigned int>(textureSize));
  glBindImageTexture(0, texture.id, 0, GL_TRUE, 0, GL_WRITE_ONLY, params.format);
  glDispatchCompute(numGroups, numGroups, 6);
  glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
  return texture;
}
Texture RenderingSystem::CreatePrefilterMapFromCubeMap(Texture cubeMap, const int textureSize) {
  constexpr unsigned int workgroupSize = 8;
  const auto mipLevels = static_cast<unsigned int>(std::floor(std::log2(textureSize))) + 1;
  Texture texture{};
  texture.type = TextureType::Prefilter;
  texture.width = textureSize;
  texture.height = textureSize;
  TextureParams params{textureSize, textureSize, GL_TEXTURE_CUBE_MAP, GL_RGBA32F, 1, static_cast<int>(mipLevels)};
  texture.id = texturePool.Request(params);
  auto shader = GetCompute(ComputeType::PrefilterMap);
  if (!shader) {
    spdlog::warn("Compute shader not found: {}.", EnumTraits<ComputeType>().GetNames().at(static_cast<uint8_t>(ComputeType::PrefilterMap)));
    return texture;
  }
  shader->Use();
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_CUBE_MAP, cubeMap.id);
  shader->SetUniform("cubeMap", 0);
  shader->SetUniform("mipLevels", mipLevels);
  for (auto mip = 0; mip < mipLevels; ++mip) {
    auto mipSize = static_cast<unsigned int>(textureSize) >> mip;
    auto roughness = static_cast<float>(mip) / (mipLevels - 1);
    shader->SetUniform("roughness", roughness);
    shader->SetUniform("mipWidth", mipSize);
    shader->SetUniform("cubeSize", mipSize);
    glBindImageTexture(0, texture.id, mip, GL_TRUE, 0, GL_WRITE_ONLY, params.format);
    auto numGroups = static_cast<unsigned int>(std::ceil(static_cast<float>(mipSize) / workgroupSize));
    glDispatchCompute(numGroups, numGroups, 6);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
  }
  return texture;
}
Texture RenderingSystem::CreateBRDF_LUT(const int textureSize) {
  if (brdf.IsValid())
    return brdf;
  constexpr unsigned int workgroupSize = 8;
  const auto numGroups = static_cast<unsigned int>(std::ceil(static_cast<float>(textureSize) / workgroupSize));
  Texture texture{};
  texture.type = TextureType::BRDF;
  texture.width = textureSize;
  texture.height = textureSize;
  TextureParams params{textureSize, textureSize, GL_TEXTURE_2D, GL_RG16F};
  texture.id = texturePool.Request(params);
  auto shader = GetCompute(ComputeType::BRDF_LUT);
  if (!shader) {
    spdlog::warn("Compute shader not found: {}.", EnumTraits<ComputeType>().GetNames().at(static_cast<uint8_t>(ComputeType::BRDF_LUT)));
    return texture;
  }
  shader->Use();
  glBindImageTexture(0, texture.id, 0, GL_FALSE, 0, GL_WRITE_ONLY, params.format);
  glDispatchCompute(numGroups, numGroups, 1);
  glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
  brdf = texture;
  return brdf;
}
} // namespace kuki
