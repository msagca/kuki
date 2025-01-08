#include <asset_loader.hpp>
#include <asset_manager.hpp>
#include <component_types.hpp>
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
#include <primitive.hpp>
#include <render_system.hpp>
#include <string>
#include <uniform_location.hpp>
#include <vector>
static const auto X_AXIS = glm::vec3(1.0f, 0.0f, 0.0f);
static const auto Y_AXIS = glm::vec3(0.0f, 1.0f, 0.0f);
static const auto Z_AXIS = glm::vec3(0.0f, 0.0f, 1.0f);
static const auto WIRE_COLOR = glm::vec3(.5f);
static const auto CLEAR_COLOR = glm::vec4(.1f, .1f, .1f, 1.0f);
static const auto MAX_POINT_LIGHTS = 8;
static const auto POINT_LOCS = 7; // number of uniform locations per point light
RenderSystem::RenderSystem(EntityManager& entityManager, AssetManager& assetManager, AssetLoader& assetLoader)
  : entityManager(entityManager), assetManager(assetManager), assetLoader(assetLoader) {
  auto defaultShaderID = assetLoader.LoadShader("DefaultShader", "shader/default_lit.vert", "shader/default_lit.frag");
  auto wireframeShaderID = assetLoader.LoadShader("WireframeShader", "shader/wireframe.vert", "shader/wireframe.frag");
  auto gridShaderID = assetLoader.LoadShader("GridShader", "shader/grid.vert", "shader/grid.frag");
  // NOTE: LoadShader returns the asset ID, not the shader ID; so, we have to retrieve it from the shader component
  defaultShader = assetManager.GetComponent<Shader>(defaultShaderID)->id;
  wireframeShader = assetManager.GetComponent<Shader>(wireframeShaderID)->id;
  gridShader = assetManager.GetComponent<Shader>(gridShaderID)->id;
  SetUniformLocations(defaultShader);
  std::vector<Vertex> vertices = {{{-1.0f, .0f, -1.0f}}, {{1.0f, .0f, -1.0f}}, {{1.0f, .0f, 1.0f}}, {{-1.0f, .0f, 1.0f}}};
  std::vector<unsigned int> indices = {0, 2, 1, 2, 0, 3};
  auto meshID = assetLoader.LoadMesh("GridMesh", vertices, indices);
  gridMesh = *assetManager.GetComponent<Mesh>(meshID);
}
void RenderSystem::SetCamera(Camera* camera) {
  this->camera = camera;
}
void RenderSystem::ToggleWireframeMode() {
  wireframeMode = !wireframeMode;
  if (wireframeMode)
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
  else
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}
void RenderSystem::SetUniformLocations(unsigned int shader) {
  shaderToUniform[shader][static_cast<unsigned int>(UniformLocation::MODEL_MATRIX)] = glGetUniformLocation(shader, "model");
  shaderToUniform[shader][static_cast<unsigned int>(UniformLocation::VIEW_MATRIX)] = glGetUniformLocation(shader, "view");
  shaderToUniform[shader][static_cast<unsigned int>(UniformLocation::PROJ_MATRIX)] = glGetUniformLocation(shader, "projection");
  shaderToUniform[shader][static_cast<unsigned int>(UniformLocation::VIEW_POS)] = glGetUniformLocation(shader, "viewPos");
  shaderToUniform[shader][static_cast<unsigned int>(UniformLocation::MAT_DIFFUSE)] = glGetUniformLocation(shader, "material.diffuse");
  shaderToUniform[shader][static_cast<unsigned int>(UniformLocation::MAT_SPECULAR)] = glGetUniformLocation(shader, "material.specular");
  shaderToUniform[shader][static_cast<unsigned int>(UniformLocation::MAT_SHININESS)] = glGetUniformLocation(shader, "material.shininess");
  shaderToUniform[shader][static_cast<unsigned int>(UniformLocation::SUN_DIRECTION)] = glGetUniformLocation(shader, "directionalLight.direction");
  shaderToUniform[shader][static_cast<unsigned int>(UniformLocation::SUN_AMBIENT)] = glGetUniformLocation(shader, "directionalLight.ambient");
  shaderToUniform[shader][static_cast<unsigned int>(UniformLocation::SUN_DIFFUSE)] = glGetUniformLocation(shader, "directionalLight.diffuse");
  shaderToUniform[shader][static_cast<unsigned int>(UniformLocation::SUN_SPECULAR)] = glGetUniformLocation(shader, "directionalLight.specular");
  shaderToUniform[shader][static_cast<unsigned int>(UniformLocation::HAS_SUN)] = glGetUniformLocation(shader, "hasDirectionalLight");
  shaderToUniform[shader][static_cast<unsigned int>(UniformLocation::NUM_POINT_LIGHTS)] = glGetUniformLocation(shader, "numPointLights");
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
static glm::mat4 GetWorldTransform(const Transform const* transform) {
  auto model = glm::mat4(1.0f);
  model = glm::scale(model, transform->scale);
  model = glm::rotate(model, transform->rotation.x, X_AXIS);
  model = glm::rotate(model, transform->rotation.y, Y_AXIS);
  model = glm::rotate(model, transform->rotation.z, Z_AXIS);
  model = glm::translate(model, transform->position);
  if (transform->parent)
    // TODO: make sure there are no cyclic relations
    return GetWorldTransform(transform->parent) * model;
  return model;
}
void RenderSystem::RenderGrid() {
  GLint polygonMode[2];
  glGetIntegerv(GL_POLYGON_MODE, polygonMode);
  glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
  glUseProgram(gridShader);
  auto loc = glGetUniformLocation(gridShader, "view");
  glUniformMatrix4fv(loc, 1, GL_FALSE, glm::value_ptr(camera->view));
  loc = glGetUniformLocation(gridShader, "projection");
  glUniformMatrix4fv(loc, 1, GL_FALSE, glm::value_ptr(camera->projection));
  loc = glGetUniformLocation(gridShader, "cameraPos");
  glUniform3fv(loc, 1, glm::value_ptr(camera->position));
  loc = glGetUniformLocation(gridShader, "cameraFront");
  glUniform3fv(loc, 1, glm::value_ptr(camera->front));
  loc = glGetUniformLocation(gridShader, "cameraFOV");
  glUniform1f(loc, camera->fov);
  glBindVertexArray(gridMesh.vertexArray);
  glDrawElements(GL_TRIANGLES, gridMesh.indexCount, GL_UNSIGNED_INT, 0);
  glPolygonMode(GL_FRONT_AND_BACK, polygonMode[0]);
}
void RenderSystem::RenderObjects() {
  entityManager.ForEach<Transform, MeshFilter, MeshRenderer>([&](Transform* transform, MeshFilter* filter, MeshRenderer* renderer) {
    auto shader = defaultShader;
    glUseProgram(shader);
    auto model = GetWorldTransform(transform);
    auto& uniformLocations = shaderToUniform[shader];
    glUniformMatrix4fv(uniformLocations[static_cast<unsigned int>(UniformLocation::MODEL_MATRIX)], 1, GL_FALSE, glm::value_ptr(model));
    glUniformMatrix4fv(uniformLocations[static_cast<unsigned int>(UniformLocation::VIEW_MATRIX)], 1, GL_FALSE, glm::value_ptr(camera->view));
    glUniformMatrix4fv(uniformLocations[static_cast<unsigned int>(UniformLocation::PROJ_MATRIX)], 1, GL_FALSE, glm::value_ptr(camera->projection));
    glUniform3fv(uniformLocations[static_cast<unsigned int>(UniformLocation::VIEW_POS)], 1, glm::value_ptr(camera->position));
    glUniform3fv(uniformLocations[static_cast<unsigned int>(UniformLocation::MAT_DIFFUSE)], 1, glm::value_ptr(renderer->material.diffuse));
    glUniform3fv(uniformLocations[static_cast<unsigned int>(UniformLocation::MAT_SPECULAR)], 1, glm::value_ptr(renderer->material.specular));
    glUniform1f(uniformLocations[static_cast<unsigned int>(UniformLocation::MAT_SHININESS)], renderer->material.shininess);
    auto hasDirectionalLight = false;
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
        hasDirectionalLight = true;
      }
    });
    glUniform1i(uniformLocations[static_cast<unsigned int>(UniformLocation::HAS_SUN)], hasDirectionalLight);
    glUniform1i(uniformLocations[static_cast<unsigned int>(UniformLocation::NUM_POINT_LIGHTS)], pointLightCount);
    glBindVertexArray(filter->mesh.vertexArray);
    if (filter->mesh.indexCount > 0)
      glDrawElements(GL_TRIANGLES, filter->mesh.indexCount, GL_UNSIGNED_INT, 0);
    else
      glDrawArrays(GL_TRIANGLES, 0, filter->mesh.vertexCount);
  });
}
void RenderSystem::Update() {
  if (!camera) {
    auto cameraPtr = entityManager.GetFirstComponent<Camera>();
    if (cameraPtr)
      SetCamera(cameraPtr);
    if (!camera)
      return;
  }
  glClearColor(CLEAR_COLOR.x, CLEAR_COLOR.y, CLEAR_COLOR.z, CLEAR_COLOR.w);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  RenderObjects();
  RenderGrid();
}
