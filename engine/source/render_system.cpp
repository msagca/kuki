#include <asset_manager.hpp>
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
#include <string>
#include <uniform_location.hpp>
static const auto IDENTITY_MATRIX = glm::mat4(1.0f);
static const auto X_AXIS = glm::vec3(1.0f, 0.0f, 0.0f);
static const auto Y_AXIS = glm::vec3(0.0f, 1.0f, 0.0f);
static const auto Z_AXIS = glm::vec3(0.0f, 0.0f, 1.0f);
static const auto CLEAR_COLOR = glm::vec4(.1f, .1f, .1f, 1.0f);
static const auto MAX_POINT_LIGHTS = 8;
static const auto POINT_LOCS = 7; // number of uniform locations per point light
RenderSystem::RenderSystem(AssetManager& assetManager)
  : assetManager(assetManager) {}
void RenderSystem::Start() {
  auto shader = assetManager.GetComponent<Shader>(assetManager.GetID("DefaultLit"));
  if (shader) {
    defaultLit = shader->id;
    SetUniformLocations(defaultLit);
  } else
    std::cerr << "'Default Lit' shader is not loaded." << std::endl;
  shader = assetManager.GetComponent<Shader>(assetManager.GetID("DefaultUnlit"));
  if (shader) {
    defaultUnlit = shader->id;
    SetUniformLocations(defaultUnlit);
  } else
    std::cerr << "'Default Unlit' shader is not loaded." << std::endl;
  auto width = 800;
  auto height = 600;
  ResizeBuffers(width, height);
}
void RenderSystem::Update(float deltaTime, Scene* scene) {
  activeScene = scene;
}
void RenderSystem::Shutdown() {
  glDeleteFramebuffers(1, &fbo);
  glDeleteRenderbuffers(1, &rbo);
  glDeleteTextures(1, &colorTexture);
}
bool RenderSystem::ResizeBuffers(int width, int height) {
  // create color texture
  glGenTextures(1, &colorTexture);
  glBindTexture(GL_TEXTURE_2D, colorTexture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
  glBindTexture(GL_TEXTURE_2D, 0);
  // create depth buffer
  glGenRenderbuffers(1, &rbo);
  glBindRenderbuffer(GL_RENDERBUFFER, rbo);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
  glBindRenderbuffer(GL_RENDERBUFFER, 0);
  // create framebuffer
  glGenFramebuffers(1, &fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  // attach to framebuffer
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTexture, 0);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
    std::cerr << "Framebuffer is not complete." << std::endl;
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
void RenderSystem::SetUniformLocations(unsigned int shader) {
  shaderToUniform[shader][static_cast<unsigned int>(UniformLocation::MATRIX_MODEL)] = glGetUniformLocation(shader, "model");
  shaderToUniform[shader][static_cast<unsigned int>(UniformLocation::MATRIX_VIEW)] = glGetUniformLocation(shader, "view");
  shaderToUniform[shader][static_cast<unsigned int>(UniformLocation::MATRIX_PROJ)] = glGetUniformLocation(shader, "projection");
  shaderToUniform[shader][static_cast<unsigned int>(UniformLocation::VIEW_POS)] = glGetUniformLocation(shader, "viewPos");
  shaderToUniform[shader][static_cast<unsigned int>(UniformLocation::MATERIAL_BASE)] = glGetUniformLocation(shader, "material.base");
  shaderToUniform[shader][static_cast<unsigned int>(UniformLocation::MATERIAL_NORMAL)] = glGetUniformLocation(shader, "material.normal");
  shaderToUniform[shader][static_cast<unsigned int>(UniformLocation::MATERIAL_ORM)] = glGetUniformLocation(shader, "material.orm");
  shaderToUniform[shader][static_cast<unsigned int>(UniformLocation::MATERIAL_METALNESS)] = glGetUniformLocation(shader, "material.metalness");
  shaderToUniform[shader][static_cast<unsigned int>(UniformLocation::MATERIAL_OCCLUSION)] = glGetUniformLocation(shader, "material.occlusion");
  shaderToUniform[shader][static_cast<unsigned int>(UniformLocation::MATERIAL_ROUGHNESS)] = glGetUniformLocation(shader, "material.roughness");
  shaderToUniform[shader][static_cast<unsigned int>(UniformLocation::SUN_DIRECTION)] = glGetUniformLocation(shader, "dirLight.direction");
  shaderToUniform[shader][static_cast<unsigned int>(UniformLocation::SUN_AMBIENT)] = glGetUniformLocation(shader, "dirLight.ambient");
  shaderToUniform[shader][static_cast<unsigned int>(UniformLocation::SUN_DIFFUSE)] = glGetUniformLocation(shader, "dirLight.diffuse");
  shaderToUniform[shader][static_cast<unsigned int>(UniformLocation::SUN_SPECULAR)] = glGetUniformLocation(shader, "dirLight.specular");
  shaderToUniform[shader][static_cast<unsigned int>(UniformLocation::HAS_SUN)] = glGetUniformLocation(shader, "hasDirLight");
  shaderToUniform[shader][static_cast<unsigned int>(UniformLocation::POINT_COUNT)] = glGetUniformLocation(shader, "pointLightCount");
  for (auto i = 0; i < MAX_POINT_LIGHTS; ++i) {
    auto index = std::to_string(i);
    shaderToUniform[shader][static_cast<unsigned int>(UniformLocation::POINT_POS_0) + i * POINT_LOCS] = glGetUniformLocation(shader, ("pointLights[" + index + "].position").c_str());
    shaderToUniform[shader][static_cast<unsigned int>(UniformLocation::POINT_AMBIENT_0) + i * POINT_LOCS] = glGetUniformLocation(shader, ("pointLights[" + index + "].ambient").c_str());
    shaderToUniform[shader][static_cast<unsigned int>(UniformLocation::POINT_DIFFUSE_0) + i * POINT_LOCS] = glGetUniformLocation(shader, ("pointLights[" + index + "].diffuse").c_str());
    shaderToUniform[shader][static_cast<unsigned int>(UniformLocation::POINT_SPECULAR_0) + i * POINT_LOCS] = glGetUniformLocation(shader, ("pointLights[" + index + "].specular").c_str());
    shaderToUniform[shader][static_cast<unsigned int>(UniformLocation::POINT_CONSTANT_0) + i * POINT_LOCS] = glGetUniformLocation(shader, ("pointLights[" + index + "].constant").c_str());
    shaderToUniform[shader][static_cast<unsigned int>(UniformLocation::POINT_LINEAR_0) + i * POINT_LOCS] = glGetUniformLocation(shader, ("pointLights[" + index + "].linear").c_str());
    shaderToUniform[shader][static_cast<unsigned int>(UniformLocation::POINT_QUADRATIC_0) + i * POINT_LOCS] = glGetUniformLocation(shader, ("pointLights[" + index + "].quadratic").c_str());
  }
}
glm::mat4 RenderSystem::GetWorldTransform(const Transform* transform) {
  auto model = glm::mat4(1.0f);
  model = glm::scale(model, transform->scale);
  model = glm::rotate(model, transform->rotation.x, X_AXIS);
  model = glm::rotate(model, transform->rotation.y, Y_AXIS);
  model = glm::rotate(model, transform->rotation.z, Z_AXIS);
  model = glm::translate(model, transform->position);
  auto& entityManager = activeScene->GetEntityManager();
  if (auto parent = entityManager.GetComponent<Transform>(transform->parent))
    // TODO: make sure there are no cyclic relations
    return GetWorldTransform(parent) * model;
  return model;
}
void RenderSystem::DrawObjects(Camera& camera) {
  auto& entityManager = activeScene->GetEntityManager();
  entityManager.ForEach<Transform, MeshFilter, MeshRenderer>([&](Transform* transform, MeshFilter* filter, MeshRenderer* renderer) {
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, renderer->material.base);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, renderer->material.normal);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, renderer->material.orm);
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, renderer->material.metalness);
    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_2D, renderer->material.occlusion);
    glActiveTexture(GL_TEXTURE5);
    glBindTexture(GL_TEXTURE_2D, renderer->material.roughness);
    auto shader = defaultLit;
    glUseProgram(shader);
    auto model = GetWorldTransform(transform);
    auto& uniformLocations = shaderToUniform[shader];
    glUniformMatrix4fv(uniformLocations[static_cast<unsigned int>(UniformLocation::MATRIX_MODEL)], 1, GL_FALSE, glm::value_ptr(model));
    glUniformMatrix4fv(uniformLocations[static_cast<unsigned int>(UniformLocation::MATRIX_VIEW)], 1, GL_FALSE, glm::value_ptr(camera.view));
    glUniformMatrix4fv(uniformLocations[static_cast<unsigned int>(UniformLocation::MATRIX_PROJ)], 1, GL_FALSE, glm::value_ptr(camera.projection));
    glUniform3fv(uniformLocations[static_cast<unsigned int>(UniformLocation::VIEW_POS)], 1, glm::value_ptr(camera.position));
    glUniform1ui(uniformLocations[static_cast<unsigned int>(UniformLocation::MATERIAL_BASE)], 0);
    glUniform1ui(uniformLocations[static_cast<unsigned int>(UniformLocation::MATERIAL_NORMAL)], 1);
    glUniform1ui(uniformLocations[static_cast<unsigned int>(UniformLocation::MATERIAL_ORM)], 2);
    glUniform1ui(uniformLocations[static_cast<unsigned int>(UniformLocation::MATERIAL_METALNESS)], 3);
    glUniform1ui(uniformLocations[static_cast<unsigned int>(UniformLocation::MATERIAL_OCCLUSION)], 4);
    glUniform1ui(uniformLocations[static_cast<unsigned int>(UniformLocation::MATERIAL_ROUGHNESS)], 5);
    auto hasDirLight = false;
    auto pointLightCount = 0;
    entityManager.ForEach<Light>([&](Light* light) {
      if (light->type == LightType::Point) {
        glUniform3fv(uniformLocations[static_cast<unsigned int>(UniformLocation::POINT_POS_0) + pointLightCount * POINT_LOCS], 1, glm::value_ptr(light->vector));
        glUniform3fv(uniformLocations[static_cast<unsigned int>(UniformLocation::POINT_AMBIENT_0) + pointLightCount * POINT_LOCS], 1, glm::value_ptr(light->ambient));
        glUniform3fv(uniformLocations[static_cast<unsigned int>(UniformLocation::POINT_DIFFUSE_0) + pointLightCount * POINT_LOCS], 1, glm::value_ptr(light->diffuse));
        glUniform3fv(uniformLocations[static_cast<unsigned int>(UniformLocation::POINT_SPECULAR_0) + pointLightCount * POINT_LOCS], 1, glm::value_ptr(light->specular));
        glUniform1f(uniformLocations[static_cast<unsigned int>(UniformLocation::POINT_CONSTANT_0) + pointLightCount * POINT_LOCS], light->constant);
        glUniform1f(uniformLocations[static_cast<unsigned int>(UniformLocation::POINT_LINEAR_0) + pointLightCount * POINT_LOCS], light->linear);
        glUniform1f(uniformLocations[static_cast<unsigned int>(UniformLocation::POINT_QUADRATIC_0) + pointLightCount * POINT_LOCS], light->quadratic);
        pointLightCount++;
      } else {
        glUniform3fv(uniformLocations[static_cast<unsigned int>(UniformLocation::SUN_DIRECTION)], 1, glm::value_ptr(light->vector));
        glUniform3fv(uniformLocations[static_cast<unsigned int>(UniformLocation::SUN_AMBIENT)], 1, glm::value_ptr(light->ambient));
        glUniform3fv(uniformLocations[static_cast<unsigned int>(UniformLocation::SUN_DIFFUSE)], 1, glm::value_ptr(light->diffuse));
        glUniform3fv(uniformLocations[static_cast<unsigned int>(UniformLocation::SUN_SPECULAR)], 1, glm::value_ptr(light->specular));
        hasDirLight = true;
      }
    });
    glUniform1i(uniformLocations[static_cast<unsigned int>(UniformLocation::HAS_SUN)], hasDirLight);
    glUniform1i(uniformLocations[static_cast<unsigned int>(UniformLocation::POINT_COUNT)], pointLightCount);
    glBindVertexArray(filter->mesh.vertexArray);
    if (filter->mesh.indexCount > 0)
      glDrawElements(GL_TRIANGLES, filter->mesh.indexCount, GL_UNSIGNED_INT, 0);
    else
      glDrawArrays(GL_TRIANGLES, 0, filter->mesh.vertexCount);
  });
}
void RenderSystem::DrawAsset(unsigned int id) {
  auto [transform, mesh, material] = assetManager.GetComponents<Transform, Mesh, Material>(id);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, material->base);
  auto shader = defaultUnlit;
  glUseProgram(shader);
  auto model = GetWorldTransform(transform);
  auto& uniformLocations = shaderToUniform[shader];
  glUniformMatrix4fv(uniformLocations[static_cast<unsigned int>(UniformLocation::MATRIX_MODEL)], 1, GL_FALSE, glm::value_ptr(model));
  glUniformMatrix4fv(uniformLocations[static_cast<unsigned int>(UniformLocation::MATRIX_VIEW)], 1, GL_FALSE, glm::value_ptr(assetCam.view));
  glUniformMatrix4fv(uniformLocations[static_cast<unsigned int>(UniformLocation::MATRIX_PROJ)], 1, GL_FALSE, glm::value_ptr(assetCam.projection));
  glUniform3fv(uniformLocations[static_cast<unsigned int>(UniformLocation::VIEW_POS)], 1, glm::value_ptr(assetCam.position));
  glUniform1ui(uniformLocations[static_cast<unsigned int>(UniformLocation::MATERIAL_BASE)], 0);
  glBindVertexArray(mesh->vertexArray);
  if (mesh->indexCount > 0)
    glDrawElements(GL_TRIANGLES, mesh->indexCount, GL_UNSIGNED_INT, 0);
  else
    glDrawArrays(GL_TRIANGLES, 0, mesh->vertexCount);
}
void RenderSystem::DrawGizmos(Camera& camera, int entity) {
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
int RenderSystem::RenderSceneToTexture(Camera& camera, int width, int height, int entity) {
  static int currentWidth, currentHeight;
  if (width != currentWidth || height != currentHeight) {
    currentWidth = width;
    currentHeight = height;
    if (!ResizeBuffers(width, height))
      return -1;
  }
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glClearColor(CLEAR_COLOR.x, CLEAR_COLOR.y, CLEAR_COLOR.z, CLEAR_COLOR.w);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  DrawObjects(camera);
  //DrawGizmos(camera, entity);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  return colorTexture;
}
int RenderSystem::RenderAssetToTexture(unsigned int id, int size) {
  static int currentSize;
  if (size != currentSize) {
    currentSize = size;
    if (!ResizeBuffers(size, size))
      return -1;
  }
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glClearColor(CLEAR_COLOR.x, CLEAR_COLOR.y, CLEAR_COLOR.z, CLEAR_COLOR.w);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  DrawAsset(id);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  return colorTexture;
}
