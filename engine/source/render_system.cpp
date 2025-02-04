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
  auto defaultShader = assetManager.GetComponent<Shader>(assetManager.GetID("DefaultShader"));
  if (defaultShader) {
    defaultShaderID = defaultShader->id;
    SetUniformLocations(defaultShaderID);
  } else
    std::cerr << "Default shader asset is not loaded." << std::endl;
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
    auto shader = defaultShaderID;
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
int RenderSystem::RenderToTexture(Camera& camera, int width, int height) {
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
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  return colorTexture;
}
