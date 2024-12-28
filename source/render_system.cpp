#include <asset_manager.hpp>
#include <component_types.hpp>
#include <entity_manager.hpp>
#include <format>
#include <glad/glad.h>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/matrix_transform.inl>
#include <glm/ext/vector_float3.hpp>
#include <glm/ext/vector_float4.hpp>
#include <glm/gtc/type_ptr.inl>
#include <primitive.hpp>
#include <string>
#include <system.hpp>
#include <vector>
static const auto X_AXIS = glm::vec3(1.0f, 0.0f, 0.0f);
static const auto Y_AXIS = glm::vec3(0.0f, 1.0f, 0.0f);
static const auto Z_AXIS = glm::vec3(0.0f, 0.0f, 1.0f);
static const auto MAX_LIGHT_SOURCES = 8;
static const auto WIRE_COLOR = glm::vec3(.5f);
static const auto CLEAR_COLOR = glm::vec4(.1f, .1f, .1f, 1.0f);
RenderSystem::RenderSystem(EntityManager& entityManager, AssetManager& assetManager, AssetLoader& assetLoader)
  : entityManager(entityManager), assetManager(assetManager), assetLoader(assetLoader) {
  auto defaultShaderID = assetLoader.LoadShader("DefaultShader", "shader/default_lit.vert", "shader/default_lit.frag");
  auto wireframeShaderID = assetLoader.LoadShader("WireframeShader", "shader/wireframe.vert", "shader/wireframe.frag");
  auto gridShaderID = assetLoader.LoadShader("GridShader", "shader/grid.vert", "shader/grid.frag");
  defaultShader = assetManager.GetComponent<Shader>(defaultShaderID).id;
  wireframeShader = assetManager.GetComponent<Shader>(wireframeShaderID).id;
  gridShader = assetManager.GetComponent<Shader>(gridShaderID).id;
  std::vector<Vertex> vertices = {{{-1.0f, .0f, -1.0f}, {}}, {{1.0f, .0f, -1.0f}, {}}, {{1.0f, .0f, 1.0f}, {}}, {{-1.0f, .0f, 1.0f}, {}}};
  std::vector<unsigned int> indices = {0, 2, 1, 2, 0, 3};
  auto meshID = assetLoader.LoadMesh("GridMesh", vertices, indices);
  gridMesh = assetManager.GetComponent<Mesh>(meshID);
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
static glm::mat4 GetWorldTransform(const Transform& transform) {
  auto model = glm::mat4(1.0f);
  model = glm::scale(model, transform.scale);
  model = glm::rotate(model, transform.rotation.x, X_AXIS);
  model = glm::rotate(model, transform.rotation.y, Y_AXIS);
  model = glm::rotate(model, transform.rotation.z, Z_AXIS);
  model = glm::translate(model, transform.position);
  if (transform.parent)
    // TODO: make sure there are no cyclic relations
    return GetWorldTransform(*transform.parent) * model;
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
  glBindVertexArray(gridMesh.vertexArray);
  glDrawElements(GL_TRIANGLES, gridMesh.indexCount, GL_UNSIGNED_INT, 0);
  glPolygonMode(GL_FRONT_AND_BACK, polygonMode[0]);
}
void RenderSystem::RenderObjects() {
  entityManager.ForEach<Transform, MeshFilter, MeshRenderer>([&](Transform& transform, MeshFilter& filter, MeshRenderer& renderer) {
    // TODO: do instanced rendering for objects sharing the same mesh
    auto shader = defaultShader;
    glUseProgram(shader);
    auto model = GetWorldTransform(transform);
    auto loc = glGetUniformLocation(shader, "model");
    glUniformMatrix4fv(loc, 1, GL_FALSE, glm::value_ptr(model));
    loc = glGetUniformLocation(shader, "view");
    glUniformMatrix4fv(loc, 1, GL_FALSE, glm::value_ptr(camera->view));
    loc = glGetUniformLocation(shader, "projection");
    glUniformMatrix4fv(loc, 1, GL_FALSE, glm::value_ptr(camera->projection));
    loc = glGetUniformLocation(shader, "viewPos");
    glUniform3fv(loc, 1, glm::value_ptr(camera->position));
    loc = glGetUniformLocation(shader, "material.diffuse");
    glUniform3fv(loc, 1, glm::value_ptr(renderer.material.diffuse));
    loc = glGetUniformLocation(shader, "material.specular");
    glUniform3fv(loc, 1, glm::value_ptr(renderer.material.specular));
    loc = glGetUniformLocation(shader, "material.shininess");
    glUniform1f(loc, renderer.material.shininess);
    auto hasDirectionalLight = false;
    auto pointLightCount = 0;
    entityManager.ForEach<Light>([&](Light& light) {
      if (light.type == LightType::Point) {
        // TODO: cache the locations to avoid std::format calls if possible
        loc = glGetUniformLocation(shader, std::format("pointLights[{}].position", pointLightCount).c_str());
        glUniform3fv(loc, 1, glm::value_ptr(light.vector));
        loc = glGetUniformLocation(shader, std::format("pointLights[{}].ambient", pointLightCount).c_str());
        glUniform3fv(loc, 1, glm::value_ptr(light.ambient));
        loc = glGetUniformLocation(shader, std::format("pointLights[{}].diffuse", pointLightCount).c_str());
        glUniform3fv(loc, 1, glm::value_ptr(light.diffuse));
        loc = glGetUniformLocation(shader, std::format("pointLights[{}].specular", pointLightCount).c_str());
        glUniform3fv(loc, 1, glm::value_ptr(light.specular));
        loc = glGetUniformLocation(shader, std::format("pointLights[{}].constant", pointLightCount).c_str());
        glUniform1f(loc, light.constant);
        loc = glGetUniformLocation(shader, std::format("pointLights[{}].linear", pointLightCount).c_str());
        glUniform1f(loc, light.linear);
        loc = glGetUniformLocation(shader, std::format("pointLights[{}].quadratic", pointLightCount).c_str());
        glUniform1f(loc, light.quadratic);
        pointLightCount++;
      } else { // NOTE: if there are multiple directional lights, only the last one in the list will have an impact
        loc = glGetUniformLocation(shader, "directionalLight.direction");
        glUniform3fv(loc, 1, glm::value_ptr(light.vector));
        loc = glGetUniformLocation(shader, "directionalLight.ambient");
        glUniform3fv(loc, 1, glm::value_ptr(light.ambient));
        loc = glGetUniformLocation(shader, "directionalLight.diffuse");
        glUniform3fv(loc, 1, glm::value_ptr(light.diffuse));
        loc = glGetUniformLocation(shader, "directionalLight.specular");
        glUniform3fv(loc, 1, glm::value_ptr(light.specular));
        hasDirectionalLight = true;
      }
    });
    loc = glGetUniformLocation(shader, "hasDirectionalLight");
    glUniform1i(loc, hasDirectionalLight);
    loc = glGetUniformLocation(shader, "numPointLights");
    glUniform1i(loc, pointLightCount);
    glBindVertexArray(filter.mesh.vertexArray);
    if (filter.mesh.indexCount > 0)
      glDrawElements(GL_TRIANGLES, filter.mesh.indexCount, GL_UNSIGNED_INT, 0);
    else
      glDrawArrays(GL_TRIANGLES, 0, filter.mesh.vertexCount);
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
  RenderGrid();
  RenderObjects();
}
