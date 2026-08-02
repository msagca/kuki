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
#include <primitive.hpp>
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
GLRenderer::GLRenderer(Application &app)
  : Renderer(std::in_place_type<GLRenderer>, app) {}
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
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  bloomShader->Use();
  bloomShader->SetTexture("u_image", in0->texture);
  bloomShader->SetTexture("u_imageBright", in1->texture);
  bloomShader->SetUniform("u_intensity", .5f);
  bloomShader->SetUniform("u_model", glm::mat4(1.f));
  bloomShader->Draw(*mesh);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
auto GLRenderer::ApplyBlurEffect(std::span<std::string> inputs, std::span<std::string> outputs) -> void {
  constexpr auto NUM_PASSES = 8;
  if (inputs.size() != 1 || outputs.size() != 1)
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
  blurShader->Use();
  blurShader->SetUniform("u_model", glm::mat4(1.f));
  for (auto i = 0; i < NUM_PASSES; ++i) {
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
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  brightShader->Use();
  brightShader->SetTexture("u_image", in->texture);
  brightShader->SetUniform("u_threshold", .5f);
  brightShader->SetUniform("u_model", glm::mat4(1.f));
  brightShader->Draw(*mesh);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
auto GLRenderer::ApplyGammaCorrection(std::span<std::string> inputs, std::span<std::string> outputs) -> void {
  auto gammaShader = GetShader("GammaCorrect");
  if (!gammaShader)
    return;
  const auto mesh = GetPrimitive("Frame");
  if (!mesh)
    return;
  const auto in = GetTarget(inputs, "SceneOutlined");
  const auto out = GetTarget(outputs, "SceneSRGB");
  if (!in || !out)
    return;
  glBindFramebuffer(GL_FRAMEBUFFER, out->framebuffer);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  gammaShader->Use();
  gammaShader->SetTexture("u_image", in->texture);
  gammaShader->SetUniform("u_gamma", 2.2f);
  gammaShader->SetUniform("u_model", glm::mat4(1.f));
  gammaShader->Draw(*mesh);
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
  const auto sceneMulti = GetTarget(inputs, "SceneMulti");
  const auto in = GetTarget(inputs, "Scene");
  const auto out = GetTarget(outputs, "SceneOutlined");
  if (!sceneMulti || !in || !out || sceneMulti->idTexture == 0)
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
  DrawEntities(inputs);
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
  glBindFramebuffer(GL_READ_FRAMEBUFFER, out->framebuffer);
  glReadBuffer(GL_COLOR_ATTACHMENT1);
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, pickFramebuffer);
  glDrawBuffer(GL_COLOR_ATTACHMENT0);
  glBlitFramebuffer(x, flippedY, x + 1, flippedY + 1, 0, 0, 1, 1, GL_COLOR_BUFFER_BIT, GL_NEAREST);
  glReadBuffer(GL_COLOR_ATTACHMENT0);
  glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
  unsigned char pixel[4];
  glGetTextureImage(pickTexture, 0, GL_RGBA, GL_UNSIGNED_BYTE, sizeof(pixel), pixel);
  const auto value = static_cast<uint32_t>(pixel[0]) | static_cast<uint32_t>(pixel[1]) << 8 | static_cast<uint32_t>(pixel[2]) << 16;
  if (value == 0xFFFFFFu)
    return EntityID::Invalid;
  return EntityID{static_cast<long long>(value)};
}
auto GLRenderer::DrawEntities(std::span<std::string> inputs) -> void {
  auto scene = GetScene();
  if (!scene)
    return;
  if (const auto generation = scene->GetStructuralGeneration(); generation != renderBucketGeneration) {
    RebuildRenderBuckets(*scene);
    renderBucketGeneration = generation;
  }
  auto camera = scene->GetCamera();
  for (const auto &[meshMat, ids] : renderBuckets) {
    if (ids.empty())
      continue;
    std::vector<glm::mat4> transforms;
    std::vector<MaterialFallback> fallbacks;
    std::vector<uint32_t> entityIds;
    transforms.reserve(ids.size());
    fallbacks.reserve(ids.size());
    entityIds.reserve(ids.size());
    for (const auto id : ids) {
      const auto transform = scene->GetEntityComponent<Transform>(id);
      if (camera) {
        const auto bounds = scene->GetEntityComponent<BoundingBox>(id);
        if (bounds && *bounds && !camera->IntersectsFrustum(bounds->GetWorldBounds(transform->world)))
          continue;
      }
      transforms.push_back(transform->world);
      fallbacks.push_back(scene->GetEntityComponent<GLMaterial>(id)->fallback);
      entityIds.push_back(static_cast<uint32_t>(static_cast<long long>(id)));
    }
    const auto material = scene->GetEntityComponent<GLMaterial>(ids.front());
    DrawEntitiesInstanced(inputs, meshMat.mesh, *material, fallbacks, transforms, entityIds);
  }
  DrawSkinnedEntities(inputs);
}
auto GLRenderer::RebuildRenderBuckets(Scene &scene) -> void {
  renderBuckets.clear();
  scene.ForEachEntity<GLMesh, GLMaterial, Transform>([&](const EntityID id, const GLMesh *mesh, const GLMaterial *material, const Transform *) {
    if (mesh->vao == 0 || mesh->skinned)
      return;
    renderBuckets[GLMeshMat{.mesh = *mesh, .material = *material}].push_back(id);
  });
}
auto GLRenderer::DrawSkinnedEntities(std::span<std::string> inputs) -> void {
  auto scene = GetScene();
  if (!scene)
    return;
  auto camera = scene->GetCamera();
  if (!camera)
    return;
  scene->ForEachEntity<ModelMeshHandle, GLMesh, GLMaterial, Transform>([&](const EntityID id, const ModelMeshHandle *handle, const GLMesh *mesh, const GLMaterial *material, const Transform *transform) {
    if (!mesh->skinned || mesh->vao == 0)
      return;
    if (const auto bounds = scene->GetEntityComponent<BoundingBox>(id); bounds && *bounds && !camera->IntersectsFrustum(bounds->GetWorldBounds(transform->world)))
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
    shader->SetMaterial(*material);
    shader->SetMaterialFallback(*mesh, material->fallback, materialBuffer->id);
    shader->SetEntityIds(*mesh, std::vector<uint32_t>{static_cast<uint32_t>(static_cast<long long>(id))}, entityIdBuffer->id);
    shader->SetBoneTransforms(boneMatrices, boneBuffer->id);
    shader->Draw(*mesh, 1);
  });
}
auto GLRenderer::DrawEntitiesInstanced(std::span<std::string> inputs, const GLMesh &mesh, const GLMaterial &material, const std::vector<MaterialFallback> &fallbacks, const std::vector<glm::mat4> &transforms, const std::vector<uint32_t> &entityIds) -> void {
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
  std::unordered_map<GLMesh, std::vector<glm::mat4>> meshToTransforms;
  scene->ForEachEntity<GLMesh, Transform>([&](const EntityID, const GLMesh *mesh, const Transform *transform) {
    if (mesh->vao == 0 || mesh->skinned)
      return;
    meshToTransforms[*mesh].push_back(transform->world);
  });
  for (const auto &[mesh, transforms] : meshToTransforms)
    DrawMeshesInstanced(mesh, transforms, shadowView, shadowProjection);
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
  scene->ForEachEntity<GLMesh, Transform>([&](const EntityID, const GLMesh *mesh, const Transform *transform) {
    if (mesh->vao == 0 || mesh->skinned)
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
  resourceRegistry.Clear();
  assetIdToResourceId.clear();
  assetIdToPreviewId.clear();
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
    spdlog::info("[GLRenderer] created buffer: {}", name);
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
    spdlog::info("[GLRenderer] created render target: {}", name);
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
    spdlog::info("[GLRenderer] created texture: {}", name);
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
  // TODO: make `GetComponent<T>` work with derived types so we can use `GLShader` as the type argument here
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
auto GLRenderer::Reset() -> void {
  glClearColor(0.f, 0.f, 0.f, 0.f);
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
auto GLRenderer::UpdateTarget(const std::string &name, const TargetDescription &desc) -> void {
  const auto id = resourceRegistry.GetID(name);
  if (!id)
    return;
  auto target = resourceRegistry.GetComponent<GLRenderTarget>(id);
  if (!target)
    return;
  target->desc = desc;
  renderbufferPool.Reallocate(desc, target->renderbuffer);
  texturePool.Reallocate(desc, target->texture);
  if (desc.pickingBuffer)
    texturePool.Reallocate({.format = TargetFormat::RGBA8, .type = desc.type, .width = desc.width, .height = desc.height, .samples = desc.samples}, target->idTexture);
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
    spdlog::warn("[GLRenderer] unsupported number of channels: {}", texture.channels);
    return glTexture;
  }
  glTexture.desc.format = GLFormatToTarget(internalFormat);
  glCreateTextures(GL_TEXTURE_2D, 1, &glTexture.id);
  auto mipmaps = 1;
  if (isSRGB)
    mipmaps = std::log2(std::max(texture.width, texture.height)) + 1;
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
  texture.data = {};
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
  spdlog::info("[GLRenderer] loaded material: {}", modelMaterial.name);
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
  spdlog::info("[GLRenderer] loaded mesh: {}", modelMesh.name);
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
  spdlog::info("[GLRenderer] loaded texture: {}", modelTexture.name);
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
auto GLRenderer::GLFormatToTarget(const unsigned int format) -> TargetFormat {
  switch (format) {
  case GL_R8:
    return TargetFormat::R8;
  case GL_RG8:
    return TargetFormat::RG8;
  case GL_RGB8:
    return TargetFormat::RGB8;
  case GL_R16F:
    return TargetFormat::R16;
  case GL_RG16F:
    return TargetFormat::RG16;
  case GL_RGB16F:
    return TargetFormat::RGB16;
  case GL_RGB32F:
    return TargetFormat::RGB32;
  case GL_RGBA8:
    return TargetFormat::RGBA8;
  case GL_RGBA16F:
    return TargetFormat::RGBA16;
  case GL_RGBA32F:
    return TargetFormat::RGBA32;
  case GL_DEPTH_COMPONENT:
    return TargetFormat::DEPTH;
  default:
    return TargetFormat::Unknown;
  }
}
auto GLRenderer::TargetFormatToGL(const TargetFormat &format) -> GLFormat {
  switch (format) {
  case TargetFormat::R8:
    return {GL_R, GL_R8};
  case TargetFormat::RG8:
    return {GL_RG, GL_RG8};
  case TargetFormat::RGB8:
    return {GL_RGB, GL_RGB8};
  case TargetFormat::R16:
    return {GL_R, GL_R16F};
  case TargetFormat::RG16:
    return {GL_RG, GL_RG16F};
  case TargetFormat::RGB16:
    return {GL_RGB, GL_RGB16F};
  case TargetFormat::RGB32:
    return {GL_RGB, GL_RGB32F};
  case TargetFormat::RGBA8:
    return {GL_RGBA, GL_RGBA8};
  case TargetFormat::RGBA16:
    return {GL_RGBA, GL_RGBA16F};
  case TargetFormat::RGBA32:
    return {GL_RGBA, GL_RGBA32F};
  case TargetFormat::DEPTH:
    return {GL_DEPTH_COMPONENT, GL_DEPTH_COMPONENT};
  default:
    return {};
  }
}
} // namespace kuki
