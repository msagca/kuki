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
#include <glm/detail/type_vec4.hpp>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/ext/vector_float4.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>
#include <ImGuizmo.h>
#include <iostream>
#include <render_system.hpp>
#include <scene.hpp>
#include <variant>
static const auto IDENTITY_MATRIX = glm::mat4(1.0f);
static const auto X_AXIS = glm::vec3(1.0f, 0.0f, 0.0f);
static const auto Y_AXIS = glm::vec3(0.0f, 1.0f, 0.0f);
static const auto Z_AXIS = glm::vec3(0.0f, 0.0f, 1.0f);
static const auto CLEAR_COLOR = glm::vec4(.1f, .1f, .1f, 1.0f);
static const auto MAX_POINT_LIGHTS = 8;
static const auto POINT_LOCS = 7; // number of uniform locations per point light
RenderSystem::RenderSystem(EntityManager& assetManager)
  : assetManager(assetManager), phongShader("shader/phong.vert", "shader/phong.frag"), pbrShader("shader/pbr.vert", "shader/pbr.frag"), unlitShader("shader/unlit.vert", "shader/unlit.frag") {}
void RenderSystem::Start() {
  assetCam.view = glm::lookAt(assetCam.position, assetCam.position + assetCam.front, assetCam.up);
  assetCam.projection = glm::perspective(assetCam.fov, assetCam.aspect, assetCam.near, assetCam.far);
}
void RenderSystem::Update(float deltaTime, Scene* scene) {
  activeScene = scene;
}
void RenderSystem::Shutdown() {
  glDeleteFramebuffers(1, &sceneFBO);
  glDeleteRenderbuffers(1, &sceneRBO);
  glDeleteTextures(1, &sceneTexture);
}
bool RenderSystem::ResizeSceneBuffers(int width, int height) {
  // create color texture
  glGenTextures(1, &sceneTexture);
  glBindTexture(GL_TEXTURE_2D, sceneTexture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
  glBindTexture(GL_TEXTURE_2D, 0);
  // create depth buffer
  glGenRenderbuffers(1, &sceneRBO);
  glBindRenderbuffer(GL_RENDERBUFFER, sceneRBO);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
  glBindRenderbuffer(GL_RENDERBUFFER, 0);
  // create framebuffer
  glGenFramebuffers(1, &sceneFBO);
  glBindFramebuffer(GL_FRAMEBUFFER, sceneFBO);
  // attach to framebuffer
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, sceneTexture, 0);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, sceneRBO);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
    std::cerr << "Scene: Framebuffer is not complete." << std::endl;
    return false;
  }
  return true;
}
bool RenderSystem::ResizeAssetBuffers(unsigned int id, int size) {
  glGenTextures(1, &id);
  glBindTexture(GL_TEXTURE_2D, id);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size, size, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
  glBindTexture(GL_TEXTURE_2D, 0);
  glGenRenderbuffers(1, &assetRBO);
  glBindRenderbuffer(GL_RENDERBUFFER, assetRBO);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, size, size);
  glBindRenderbuffer(GL_RENDERBUFFER, 0);
  glGenFramebuffers(1, &assetFBO);
  glBindFramebuffer(GL_FRAMEBUFFER, assetFBO);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, id, 0);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, assetRBO);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
    std::cerr << "Asset: Framebuffer is not complete." << std::endl;
    return false;
  }
  return true;
}
void RenderSystem::ToggleWireframeMode() {
  wireframeMode = !wireframeMode;
  if (wireframeMode)
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
  else
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}
glm::mat4 RenderSystem::GetWorldTransform(const Transform& transform) {
  auto model = glm::mat4(1.0f);
  model = glm::scale(model, transform.scale);
  model = glm::rotate(model, transform.rotation.x, X_AXIS);
  model = glm::rotate(model, transform.rotation.y, Y_AXIS);
  model = glm::rotate(model, transform.rotation.z, Z_AXIS);
  model = glm::translate(model, transform.position);
  auto& entityManager = activeScene->GetEntityManager();
  if (auto parent = entityManager.GetComponent<Transform>(transform.parent))
    // TODO: make sure there are no cyclic relations
    return GetWorldTransform(*parent) * model;
  return model;
}
void RenderSystem::DrawObject(const Transform& transform, const Mesh& mesh, const Material& material, const Camera& camera, EntityManager& entityManager) {
  auto& shader = pbrShader;
  if (std::holds_alternative<PhongMaterial>(material.material))
    shader = phongShader;
  shader.Use();
  material.Apply(shader);
  auto model = GetWorldTransform(transform);
  shader.SetMVP(model, camera);
  auto hasDirLight = false;
  auto pointLightCount = 0;
  entityManager.ForEach<Light>([&](Light* light) {
    shader.SetLight(light, pointLightCount);
    if (light->type == LightType::Directional)
      hasDirLight = true;
    else
      pointLightCount++;
  });
  shader.SetUniform("hasDirLight", hasDirLight);
  shader.SetUniform("pointLightCount", pointLightCount);
  glBindVertexArray(mesh.vertexArray);
  if (mesh.indexCount > 0)
    glDrawElements(GL_TRIANGLES, mesh.indexCount, GL_UNSIGNED_INT, 0);
  else
    glDrawArrays(GL_TRIANGLES, 0, mesh.vertexCount);
}
void RenderSystem::DrawScene(const Camera& camera) {
  auto& entityManager = activeScene->GetEntityManager();
  entityManager.ForEach<Transform, MeshFilter, MeshRenderer>([&](Transform* transform, MeshFilter* filter, MeshRenderer* renderer) {
    DrawObject(*transform, filter->mesh, renderer->material, camera, entityManager);
  });
}
void RenderSystem::DrawAsset(unsigned int id) {
  auto [transform, mesh, material] = assetManager.GetComponents<Transform, Mesh, Material>(id);
}
void RenderSystem::DrawGizmos(const Camera& camera, int entity) {
  // FIXME: for this to work, engine and editor should share the same ImGui globals (editor should pass its ImGui context to engine)
  ImGuizmo::BeginFrame();
  ImGuizmo::DrawGrid(glm::value_ptr(camera.view), glm::value_ptr(camera.projection), glm::value_ptr(IDENTITY_MATRIX), camera.far);
  if (entity < 0)
    return;
  auto& entityManager = activeScene->GetEntityManager();
  auto transform = entityManager.GetComponent<Transform>(entity);
  if (!transform)
    return;
  glm::mat4 matrix;
  auto rotation = glm::degrees(transform->rotation);
  ImGuizmo::RecomposeMatrixFromComponents(glm::value_ptr(transform->position), glm::value_ptr(rotation), glm::value_ptr(transform->scale), glm::value_ptr(matrix));
  ImGuizmo::SetOrthographic(false);
  auto displaySize = ImGui::GetIO().DisplaySize;
  ImGuizmo::SetRect(0, 0, displaySize.x, displaySize.y);
  ImGuizmo::Manipulate(glm::value_ptr(camera.view), glm::value_ptr(camera.projection), ImGuizmo::OPERATION::UNIVERSAL, ImGuizmo::MODE::WORLD, glm::value_ptr(matrix));
  ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(matrix), glm::value_ptr(transform->position), glm::value_ptr(rotation), glm::value_ptr(transform->scale));
  transform->rotation = glm::radians(rotation);
}
int RenderSystem::RenderSceneToTexture(const Camera& camera, int width, int height, int entity) {
  static int currentWidth, currentHeight;
  if (width != currentWidth || height != currentHeight) {
    currentWidth = width;
    currentHeight = height;
    if (!ResizeSceneBuffers(width, height))
      return -1;
  }
  glBindFramebuffer(GL_FRAMEBUFFER, sceneFBO);
  glClearColor(CLEAR_COLOR.x, CLEAR_COLOR.y, CLEAR_COLOR.z, CLEAR_COLOR.w);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  DrawScene(camera);
  //DrawGizmos(camera, entity);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  return sceneTexture;
}
bool RenderSystem::RenderAssetToTexture(unsigned int assetID, unsigned int textureID, int size) {
  static int currentSize;
  if (size != currentSize) {
    currentSize = size;
    if (!ResizeAssetBuffers(textureID, size))
      return false;
  }
  glBindFramebuffer(GL_FRAMEBUFFER, assetFBO);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textureID, 0);
  glClearColor(CLEAR_COLOR.x, CLEAR_COLOR.y, CLEAR_COLOR.z, CLEAR_COLOR.w);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  DrawAsset(assetID);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  return true;
}
