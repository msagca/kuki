#define GLM_ENABLE_EXPERIMENTAL
#include <application.hpp>
#include <cmath>
#include <component/camera.hpp>
#include <component/component.hpp>
#include <component/light.hpp>
#include <component/material.hpp>
#include <component/mesh.hpp>
#include <component/mesh_filter.hpp>
#include <component/mesh_renderer.hpp>
#include <component/shader.hpp>
#include <component/texture.hpp>
#include <component/transform.hpp>
#include <functional>
#include <glad/glad.h>
#include <glm/detail/qualifier.hpp>
#include <glm/detail/type_mat4x4.hpp>
#include <glm/detail/type_vec3.hpp>
#include <glm/ext/matrix_float3x3.hpp>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/ext/vector_float4.hpp>
#include <glm/gtx/quaternion.hpp>
#include <iostream>
#include <limits>
#include <render_system.hpp>
#include <system.hpp>
#include <tuple>
#include <typeindex>
#include <unordered_map>
#include <variant>
#include <vector>
#include <ios>
#include <utility>
RenderSystem::RenderSystem(Application& app)
  : System(app), texturePool(CreateTexture, DeleteTexture, 16) {}
RenderSystem::~RenderSystem() {
  for (const auto& [_, shader] : shaders)
    delete shader;
  shaders.clear();
}
void RenderSystem::Start() {
  auto phongShader = new Shader("Phong", "shader/phong.vert", "shader/phong.frag");
  auto pbrShader = new Shader("PBR", "shader/pbr.vert", "shader/pbr.frag");
  auto unlitShader = new Shader("Unlit", "shader/unlit.vert", "shader/unlit.frag");
  auto skyboxShader = new Shader("Skybox", "shader/skybox.vert", "shader/skybox.frag");
  shaders.insert({phongShader->GetName(), phongShader});
  shaders.insert({pbrShader->GetName(), pbrShader});
  shaders.insert({unlitShader->GetName(), unlitShader});
  shaders.insert({skyboxShader->GetName(), skyboxShader});
  glGenBuffers(1, &instanceVBO);
}
void RenderSystem::Shutdown() {
  glDeleteFramebuffers(1, &assetFBO);
  glDeleteFramebuffers(1, &sceneFBO);
  glDeleteFramebuffers(1, &sceneMultiFBO);
  glDeleteRenderbuffers(1, &assetRBO);
  glDeleteRenderbuffers(1, &sceneMultiRBO);
  glDeleteRenderbuffers(1, &sceneRBO);
  glDeleteTextures(1, &sceneMultiTexture);
  glDeleteTextures(1, &sceneTexture);
}
bool RenderSystem::UpdateBuffers(unsigned int& fbo, unsigned int& rbo, unsigned int& texture, int width, int height, int samples) {
  auto multi = samples > 1;
  if (texture == 0)
    glCreateTextures(multi ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D, 1, &texture);
  if (multi) {
    glTextureStorage2DMultisample(texture, samples, GL_RGBA8, width, height, GL_TRUE);
  } else {
    glTextureStorage2D(texture, 1, GL_RGBA8, width, height);
    glTextureParameteri(texture, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureParameteri(texture, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  }
  if (rbo == 0)
    glCreateRenderbuffers(1, &rbo);
  if (multi)
    glNamedRenderbufferStorageMultisample(rbo, samples, GL_DEPTH24_STENCIL8, width, height);
  else
    glNamedRenderbufferStorage(rbo, GL_DEPTH24_STENCIL8, width, height);
  if (fbo == 0)
    glCreateFramebuffers(1, &fbo);
  glNamedFramebufferTexture(fbo, GL_COLOR_ATTACHMENT0, texture, 0);
  glNamedFramebufferRenderbuffer(fbo, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo);
  auto status = glCheckNamedFramebufferStatus(fbo, GL_FRAMEBUFFER);
  if (status != GL_FRAMEBUFFER_COMPLETE) {
    std::cerr << "Framebuffer is incomplete. Status: 0x" << std::hex << status << std::dec << std::endl;
    return false;
  }
  return true;
}
void RenderSystem::ToggleWireframeMode() {
  static auto enabled = false;
  enabled = !enabled;
  if (enabled)
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
  else
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}
void RenderSystem::DrawAsset(const Transform* transform, const Mesh& mesh, const Material& material) {
  auto shader = GetMaterialShader(material);
  shader->Use();
  material.Apply(*shader);
  shader->SetUniform("useInstancing", false);
  auto model = GetAssetWorldTransform(transform);
  shader->SetMVP(model, assetCam.view, assetCam.projection);
  shader->SetUniform("viewPos", assetCam.position);
  static const Light dirLight;
  shader->SetLight(&dirLight);
  shader->SetUniform("dirExists", true);
  glBindVertexArray(mesh.vertexArray);
  if (mesh.indexCount > 0)
    glDrawElements(GL_TRIANGLES, mesh.indexCount, GL_UNSIGNED_INT, 0);
  else
    glDrawArrays(GL_TRIANGLES, 0, mesh.vertexCount);
}
void RenderSystem::DrawEntity(const Transform* transform, const Mesh& mesh, const Material& material) {
  auto shader = GetMaterialShader(material);
  shader->Use();
  material.Apply(*shader);
  shader->SetUniform("useInstancing", false);
  auto model = GetEntityWorldTransform(transform);
  shader->SetMVP(model, app.GetActiveCamera()->view, app.GetActiveCamera()->projection);
  shader->SetUniform("viewPos", app.GetActiveCamera()->position);
  auto dirExists = false;
  auto pointCount = 0;
  app.ForEachEntity<Light>([&](unsigned int id, Light* light) {
    shader->SetLight(light, pointCount);
    if (light->type == LightType::Directional)
      dirExists = true;
    else
      pointCount++;
  });
  shader->SetUniform("dirExists", dirExists);
  shader->SetUniform("pointCount", pointCount);
  glBindVertexArray(mesh.vertexArray);
  if (mesh.indexCount > 0)
    glDrawElements(GL_TRIANGLES, mesh.indexCount, GL_UNSIGNED_INT, 0);
  else
    glDrawArrays(GL_TRIANGLES, 0, mesh.vertexCount);
}
void RenderSystem::DrawEntitiesInstanced(const Mesh& mesh, const std::vector<unsigned int>& entities) {
  std::vector<Material> materials;
  std::vector<glm::mat4> models;
  for (const auto id : entities) {
    auto [renderer, transform] = app.GetComponents<MeshRenderer, Transform>(id);
    if (!renderer || !transform)
      continue;
    materials.push_back(renderer->material);
    models.push_back(GetEntityWorldTransform(transform));
  }
  if (materials.size() == 0)
    return;
  auto& material = materials[0];
  auto shader = GetMaterialShader(material);
  shader->Use();
  material.Apply(*shader);
  shader->SetUniform("useInstancing", true);
  shader->SetUniform("view", app.GetActiveCamera()->view);
  shader->SetUniform("projection", app.GetActiveCamera()->projection);
  shader->SetUniform("viewPos", app.GetActiveCamera()->position);
  auto dirExists = false;
  auto pointCount = 0;
  app.ForEachEntity<Light>([&](unsigned int id, Light* light) {
    shader->SetLight(light, pointCount);
    if (light->type == LightType::Directional)
      dirExists = true;
    else
      pointCount++;
  });
  shader->SetUniform("dirExists", dirExists);
  shader->SetUniform("pointCount", pointCount);
  glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
  glBufferData(GL_ARRAY_BUFFER, models.size() * sizeof(glm::mat4), models.data(), GL_DYNAMIC_DRAW);
  glBindVertexArray(mesh.vertexArray);
  unsigned int attribLocation = 3; // 0: i_position, 1: i_normal, 2: i_texCoord
  for (auto i = 0; i < 4; ++i) {
    glEnableVertexAttribArray(attribLocation + i);
    glVertexAttribPointer(attribLocation + i, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void*)(sizeof(glm::vec4) * i));
    glVertexAttribDivisor(attribLocation + i, 1);
  }
  if (mesh.indexCount > 0)
    glDrawElementsInstanced(GL_TRIANGLES, mesh.indexCount, GL_UNSIGNED_INT, 0, models.size());
  else
    glDrawArraysInstanced(GL_TRIANGLES, 0, mesh.vertexCount, models.size());
}
void RenderSystem::DrawScene() {
  std::unordered_map<unsigned int, std::unordered_map<std::type_index, std::vector<unsigned int>>> vaoToMatToEntities;
  std::unordered_map<unsigned int, Mesh> vaoToMesh;
  app.ForEachEntity<MeshFilter, MeshRenderer, Transform>([&](unsigned int id, MeshFilter* filter, MeshRenderer* renderer, Transform* transform) {
    auto vao = filter->mesh.vertexArray;
    vaoToMesh[vao] = filter->mesh;
    if (vaoToMatToEntities.find(vao) == vaoToMatToEntities.end())
      vaoToMatToEntities[vao] = {};
    auto mat = renderer->material.GetTypeIndex();
    if (vaoToMatToEntities[vao].find(mat) == vaoToMatToEntities[vao].end())
      vaoToMatToEntities[vao][mat] = {};
    vaoToMatToEntities[vao][mat].push_back(id);
  });
  for (const auto& [vao, matToEntities] : vaoToMatToEntities)
    for (const auto& [mat, entities] : matToEntities)
      DrawEntitiesInstanced(vaoToMesh[vao], entities);
  DrawSkybox();
}
void RenderSystem::DrawSkybox() {
  auto assetID = app.GetAssetID("CubeInverted");
  auto mesh = app.GetAssetComponent<Mesh>(assetID);
  if (!mesh)
    return;
  auto shader = shaders["Skybox"];
  shader->Use();
  auto view = glm::mat4(glm::mat3(app.GetActiveCamera()->view));
  shader->SetUniform("view", view);
  shader->SetUniform("projection", app.GetActiveCamera()->projection);
  glBindVertexArray(mesh->vertexArray);
  assetID = app.GetAssetID("Skybox");
  auto skyboxTexture = app.GetAssetComponent<Texture>(assetID);
  if (!skyboxTexture)
    return;
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_CUBE_MAP, skyboxTexture->id);
  glDepthFunc(GL_LEQUAL);
  if (mesh->indexCount > 0)
    glDrawElements(GL_TRIANGLES, mesh->indexCount, GL_UNSIGNED_INT, 0);
  else
    glDrawArrays(GL_TRIANGLES, 0, mesh->vertexCount);
  glDepthFunc(GL_LESS);
}
void RenderSystem::DrawAsset(unsigned int id) {
  auto [minBound, maxBound] = GetAssetBounds(id);
  PositionCamera(assetCam, minBound, maxBound);
  if (app.AssetHasComponents<Transform, Mesh, Material>(id)) {
    auto [transform, mesh, material] = app.GetAssetComponents<Transform, Mesh, Material>(id);
    DrawAsset(transform, *mesh, *material);
  } else if (auto texture = app.GetAssetComponent<Texture>(id)) {
    Material material;
    material.material = UnlitMaterial();
    auto& unlitMaterial = std::get<UnlitMaterial>(material.material);
    unlitMaterial.base = texture->id;
    auto frameID = app.GetAssetID("Frame");
    auto quadMesh = app.GetAssetComponent<Mesh>(frameID);
    static const Transform transform;
    DrawAsset(&transform, *quadMesh, material);
  }
  app.ForEachChildAsset(id, [this](unsigned int childID) {
    DrawAsset(childID);
  });
}
int RenderSystem::RenderSceneToTexture() {
  if (!app.GetActiveScene() || !app.GetActiveCamera())
    return -1;
  auto& config = app.GetConfig();
  auto width = config.screenWidth;
  auto height = config.screenHeight;
  app.GetActiveCamera()->SetProperty({"AspectRatio", static_cast<float>(width) / height});
  auto multiBufferComplete = UpdateBuffers(sceneMultiFBO, sceneMultiRBO, sceneMultiTexture, width, height, 4);
  auto singleBufferComplete = UpdateBuffers(sceneFBO, sceneRBO, sceneTexture, width, height);
  if (!multiBufferComplete || !singleBufferComplete) {
    std::cerr << "Failed to update framebuffers for scene rendering." << std::endl;
    return -1;
  }
  glBindFramebuffer(GL_FRAMEBUFFER, sceneMultiFBO);
  glViewport(0, 0, width, height);
  glClearColor(.0f, .0f, .0f, .0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
  DrawScene();
  glBindFramebuffer(GL_READ_FRAMEBUFFER, sceneMultiFBO);
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, sceneFBO);
  glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_LINEAR);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  return sceneTexture;
}
int RenderSystem::RenderAssetToTexture(unsigned int assetID, int size) {
  static std::unordered_map<unsigned int, unsigned int> assetToTexture;
  if (assetToTexture.find(assetID) == assetToTexture.end()) {
    auto itemID = texturePool.Request();
    assetToTexture[assetID] = *texturePool.Get(itemID);
  }
  auto textureID = assetToTexture[assetID];
  size = std::max(1, size);
  UpdateBuffers(assetFBO, assetRBO, textureID, size, size);
  glBindFramebuffer(GL_FRAMEBUFFER, assetFBO);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textureID, 0);
  glViewport(0, 0, size, size);
  glClearColor(.0f, .0f, .0f, .0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
  DrawAsset(assetID);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  return textureID;
}
unsigned int RenderSystem::CreateTexture() {
  unsigned int id;
  glGenTextures(1, &id);
  return id;
}
void RenderSystem::DeleteTexture(unsigned int id) {
  glDeleteTextures(1, &id);
}
glm::mat4 RenderSystem::GetEntityWorldTransform(const Transform* transform) {
  //if (transform->dirty) {
  auto translation = glm::translate(glm::mat4(1.0f), transform->position);
  auto rotation = glm::toMat4(transform->rotation);
  auto scale = glm::scale(glm::mat4(1.0f), transform->scale);
  transform->model = translation * rotation * scale;
  //transform->dirty = false;
  //}
  if (transform->parent >= 0)
    if (auto parent = app.GetComponent<Transform>(transform->parent))
      transform->model = GetEntityWorldTransform(parent) * transform->model;
  return transform->model;
}
glm::mat4 RenderSystem::GetAssetWorldTransform(const Transform* transform) {
  auto translation = glm::translate(glm::mat4(1.0f), transform->position);
  auto rotation = glm::toMat4(transform->rotation);
  auto scale = glm::scale(glm::mat4(1.0f), transform->scale);
  transform->model = translation * rotation * scale;
  if (transform->parent >= 0)
    if (auto parent = app.GetAssetComponent<Transform>(transform->parent))
      transform->model = GetAssetWorldTransform(parent) * transform->model;
  return transform->model;
}
glm::vec3 RenderSystem::GetAssetWorldPosition(const Transform* transform) {
  auto model = GetAssetWorldTransform(transform);
  return model[3];
}
std::tuple<glm::vec3, glm::vec3> RenderSystem::GetAssetBounds(unsigned int id) {
  auto min = glm::vec3(std::numeric_limits<float>::max());
  auto max = glm::vec3(std::numeric_limits<float>::lowest());
  std::function<void(unsigned int)> calculateBounds = [&](unsigned int assetID) {
    auto [mesh, transform] = app.GetAssetComponents<Mesh, Transform>(assetID);
    if (!mesh)
      return;
    auto childMin = mesh->minBound;
    auto childMax = mesh->maxBound;
    // TODO: consider all 8 corners for bounds calculation
    if (transform) {
      auto position = GetAssetWorldPosition(transform);
      childMin += position;
      childMax += position;
    }
    min = glm::min(min, childMin);
    max = glm::max(max, childMax);
    app.ForEachChildAsset(assetID, [&](unsigned int childId) {
      calculateBounds(childId);
    });
  };
  calculateBounds(id);
  return std::tie(min, max);
}
void RenderSystem::PositionCamera(Camera& camera, const glm::vec3& minBound, const glm::vec3& maxBound) {
  glm::vec3 center = (minBound + maxBound) * .5f;
  auto dimensions = maxBound - minBound;
  camera.position = center;
  camera.pitch = -30.0f;
  camera.yaw = -45.0f;
  camera.UpdateDirection();
  auto radius = glm::length(dimensions) * .5f;
  float fovVertical = glm::radians(camera.fov);
  float fovHorizontal = 2.0f * atan(tan(fovVertical * .5f) * camera.aspectRatio);
  auto fovMin = glm::min(fovVertical, fovHorizontal);
  auto distanceFactor = 1.2f;
  float distance = (radius / tan(fovMin * .5f)) * distanceFactor;
  camera.position -= camera.front * distance;
  camera.UpdateView();
  camera.UpdateProjection();
}
Shader* RenderSystem::GetMaterialShader(const Material& material) {
  if (std::holds_alternative<PhongMaterial>(material.material))
    return shaders["Phong"];
  else if (std::holds_alternative<UnlitMaterial>(material.material))
    return shaders["Unlit"];
  else
    return shaders["PBR"];
}
