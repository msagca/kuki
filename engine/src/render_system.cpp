#define GLM_ENABLE_EXPERIMENTAL
#include <component/camera.hpp>
#include <component/component.hpp>
#include <component/light.hpp>
#include <component/material.hpp>
#include <component/mesh.hpp>
#include <component/mesh_filter.hpp>
#include <component/mesh_renderer.hpp>
#include <component/shader.hpp>
#include <component/transform.hpp>
#include <entity_manager.hpp>
#include <glad/glad.h>
#include <glm/detail/qualifier.hpp>
#include <glm/detail/type_mat4x4.hpp>
#include <glm/detail/type_vec3.hpp>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/gtx/quaternion.hpp>
#include <iostream>
#include <render_system.hpp>
#include <scene.hpp>
#include <variant>
#include <functional>
RenderSystem::RenderSystem(EntityManager& assetManager)
  : assetManager(assetManager) {}
RenderSystem::~RenderSystem() {
  for (const auto& [_, shader] : shaders)
    delete shader;
  shaders.clear();
}
void RenderSystem::Start() {
  auto phongShader = new Shader("Phong", "shader/phong.vert", "shader/phong.frag");
  auto pbrShader = new Shader("PBR", "shader/pbr.vert", "shader/pbr.frag");
  auto unlitShader = new Shader("Unlit", "shader/unlit.vert", "shader/unlit.frag");
  shaders.insert({phongShader->GetName(), phongShader});
  shaders.insert({pbrShader->GetName(), pbrShader});
  shaders.insert({unlitShader->GetName(), unlitShader});
  assetCam.view = glm::lookAt(assetCam.position, assetCam.position + assetCam.front, assetCam.up);
  assetCam.projection = glm::perspective(assetCam.fov, assetCam.aspectRatio, assetCam.nearPlane, assetCam.farPlane);
}
void RenderSystem::Update(float deltaTime, Scene* scene) {
  activeScene = scene;
  activeCamera = scene->GetCamera();
}
void RenderSystem::Shutdown() {
  glDeleteFramebuffers(1, &sceneFBO);
  glDeleteRenderbuffers(1, &sceneRBO);
  glDeleteTextures(1, &sceneTexture);
  glDeleteFramebuffers(1, &assetFBO);
  glDeleteRenderbuffers(1, &assetRBO);
}
bool RenderSystem::ResizeBuffers(unsigned int& fbo, unsigned int& rbo, unsigned int& texture, int width, int height) {
  if (texture == 0) {
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  } else
    glBindTexture(GL_TEXTURE_2D, texture);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
  glBindTexture(GL_TEXTURE_2D, 0);
  if (rbo == 0)
    glGenRenderbuffers(1, &rbo);
  glBindRenderbuffer(GL_RENDERBUFFER, rbo);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
  glBindRenderbuffer(GL_RENDERBUFFER, 0);
  if (fbo == 0) {
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo);
  } else
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  auto status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  if (status != GL_FRAMEBUFFER_COMPLETE) {
    std::cerr << "Framebuffer is incomplete." << std::endl;
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
glm::mat4 RenderSystem::GetWorldTransform(const Transform* transform) {
  auto translation = glm::translate(glm::mat4(1.0f), transform->position);
  auto rotation = glm::toMat4(transform->rotation);
  auto scale = glm::scale(glm::mat4(1.0f), transform->scale);
  auto model = translation * rotation * scale;
  auto& entityManager = activeScene->GetEntityManager();
  if (auto parent = entityManager.GetComponent<Transform>(transform->parent))
    return GetWorldTransform(parent) * model;
  return model;
}
void RenderSystem::DrawObject(const Transform* transform, const Mesh& mesh, const Material& material) {
  if (!activeCamera)
    return;
  auto shader = shaders["PBR"];
  if (std::holds_alternative<PhongMaterial>(material.material))
    shader = shaders["Phong"];
  if (!shader)
    return;
  shader->Use();
  material.Apply(*shader);
  auto model = GetWorldTransform(transform);
  shader->SetMVP(model, *activeCamera);
  auto dirExists = false;
  auto pointCount = 0;
  auto& entityManager = activeScene->GetEntityManager();
  entityManager.ForEach<Light>([&](Light* light) {
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
void RenderSystem::DrawScene() {
  if (!activeCamera)
    return;
  auto& entityManager = activeScene->GetEntityManager();
  entityManager.ForEach<Transform, MeshFilter, MeshRenderer>([&](Transform* transform, MeshFilter* filter, MeshRenderer* renderer) {
    DrawObject(transform, filter->mesh, renderer->material);
  });
}
void RenderSystem::DrawAsset(unsigned int id) {
  auto& entityManager = activeScene->GetEntityManager();
  if (assetManager.HasComponents<Transform, Mesh, Material>(id)) {
    auto [transform, mesh, material] = assetManager.GetComponents<Transform, Mesh, Material>(id);
    DrawObject(transform, *mesh, *material);
  } else if (assetManager.HasComponent<Material>(id)) {
    auto material = assetManager.GetComponent<Material>(id);
    auto planeID = assetManager.GetID("Plane");
    auto mesh = assetManager.GetComponent<Mesh>(planeID);
    static const Transform transform;
    DrawObject(&transform, *mesh, *material);
  }
  assetManager.ForEachChild(id, [this](unsigned int childID) {
    DrawAsset(childID);
  });
}
int RenderSystem::RenderSceneToTexture(int width, int height) {
  if (!activeCamera)
    return -1;
  static int currentWidth, currentHeight;
  if (width != currentWidth || height != currentHeight) {
    currentWidth = width;
    currentHeight = height;
    if (!ResizeBuffers(sceneFBO, sceneRBO, sceneTexture, width, height))
      return -1;
  }
  glBindFramebuffer(GL_FRAMEBUFFER, sceneFBO);
  glClearColor(.0f, .0f, .0f, .0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
  DrawScene();
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  return sceneTexture;
}
bool RenderSystem::RenderAssetToTexture(unsigned int assetID, unsigned int textureID, int size) {
  static int currentSize;
  if (size != currentSize) {
    currentSize = size;
    if (!ResizeBuffers(assetFBO, assetRBO, textureID, size, size))
      return false;
  }
  glBindFramebuffer(GL_FRAMEBUFFER, assetFBO);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textureID, 0);
  glClearColor(.0f, .0f, .0f, .0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
  DrawAsset(assetID);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  return true;
}
