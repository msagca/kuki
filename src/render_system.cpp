#include <component_types.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <system.hpp>
#include <entity_manager.hpp>
#include <iostream>
static const auto X_AXIS = glm::vec3(1.0f, 0.0f, 0.0f);
static const auto Y_AXIS = glm::vec3(0.0f, 1.0f, 0.0f);
static const auto Z_AXIS = glm::vec3(0.0f, 0.0f, 1.0f);
static const auto MAX_LIGHT_SOURCES = 8;
RenderSystem::RenderSystem(EntityManager& entityManager)
  : entityManager(&entityManager) {
  defaultShader = AddShader("default_lit.vert", "default_lit.frag");
}
GLuint RenderSystem::AddShader(const char* vert, const char* frag) {
  Shader shader(vert, frag);
  shaderDB.insert(shader.ID);
  return shader.ID;
}
void RenderSystem::RemoveShader(GLuint id) {
  shaderDB.erase(id);
  glDeleteProgram(id);
}
void RenderSystem::SetCamera(Camera* camera) {
  // TODO: if there is no active camera, scan the entities for camera components
  this->camera = camera;
}
void RenderSystem::AddLight(Light* light) {
  if (lightSources.size() >= MAX_LIGHT_SOURCES) {
    std::cerr << "The number of light sources in the scene exceeded the allowed limit (" << MAX_LIGHT_SOURCES << ")." << std::endl;
    return;
  }
  lightSources.push_back(light);
}
void RenderSystem::RemoveLight(Light* light) {
  auto it = std::find(lightSources.begin(), lightSources.end(), light);
  if (it != lightSources.end())
    lightSources.erase(it);
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
void RenderSystem::Update() {
  if (!camera || lightSources.size() == 0)
    return;
  entityManager->ForEach<Transform, MeshFilter, MeshRenderer>([&](Transform& transform, MeshFilter& filter, MeshRenderer& renderer) {
    // TODO: do instanced rendering for objects sharing the same mesh
    auto shader = defaultShader;
    if (shaderDB.find(renderer.shader) != shaderDB.end())
      shader = renderer.shader;
    glUseProgram(shader);
    auto model = GetWorldTransform(transform);
    // transform matrices
    auto loc = glGetUniformLocation(shader, "model");
    glUniformMatrix4fv(loc, 1, GL_FALSE, glm::value_ptr(model));
    loc = glGetUniformLocation(shader, "view");
    glUniformMatrix4fv(loc, 1, GL_FALSE, glm::value_ptr(camera->view));
    loc = glGetUniformLocation(shader, "projection");
    glUniformMatrix4fv(loc, 1, GL_FALSE, glm::value_ptr(camera->projection));
    // material properties
    loc = glGetUniformLocation(shader, "material.diffuse");
    glUniform3fv(loc, 1, glm::value_ptr(renderer.material.diffuse));
    loc = glGetUniformLocation(shader, "material.specular");
    glUniform3fv(loc, 1, glm::value_ptr(renderer.material.specular));
    loc = glGetUniformLocation(shader, "material.shininess");
    glUniform1f(loc, renderer.material.shininess);
    // light properties
    auto hasDirectionalLight = false;
    auto numPointLights = 0;
    for (auto i = 0; i < lightSources.size(); i++)
      if (lightSources[i]->type == LightType::Point)
        numPointLights++;
      else
        hasDirectionalLight = true;
    loc = glGetUniformLocation(shader, "hasDirectionalLight");
    glUniform1i(loc, hasDirectionalLight);
    loc = glGetUniformLocation(shader, "numPointLights");
    glUniform1i(loc, numPointLights);
    for (auto i = 0, j = 0; i < lightSources.size(); i++)
      if (lightSources[i]->type == LightType::Point) {
        // TODO: cache the locations to avoid std::format calls if possible
        loc = glGetUniformLocation(shader, std::format("pointLights[{}].position", j).c_str());
        glUniform3fv(loc, 1, glm::value_ptr(lightSources[i]->vector));
        loc = glGetUniformLocation(shader, std::format("pointLights[{}].ambient", j).c_str());
        glUniform3fv(loc, 1, glm::value_ptr(lightSources[i]->ambient));
        loc = glGetUniformLocation(shader, std::format("pointLights[{}].diffuse", j).c_str());
        glUniform3fv(loc, 1, glm::value_ptr(lightSources[i]->diffuse));
        loc = glGetUniformLocation(shader, std::format("pointLights[{}].specular", j).c_str());
        glUniform3fv(loc, 1, glm::value_ptr(lightSources[i]->specular));
        loc = glGetUniformLocation(shader, std::format("pointLights[{}].constant", j).c_str());
        glUniform1f(loc, lightSources[i]->constant);
        loc = glGetUniformLocation(shader, std::format("pointLights[{}].linear", j).c_str());
        glUniform1f(loc, lightSources[i]->linear);
        loc = glGetUniformLocation(shader, std::format("pointLights[{}].quadratic", j).c_str());
        glUniform1f(loc, lightSources[i]->quadratic);
        j++;
      } else { // NOTE: if there are multiple directional lights, only the last one in the list will have an impact
        loc = glGetUniformLocation(shader, "directionalLight.direction");
        glUniform3fv(loc, 1, glm::value_ptr(lightSources[i]->vector));
        loc = glGetUniformLocation(shader, "directionalLight.ambient");
        glUniform3fv(loc, 1, glm::value_ptr(lightSources[i]->ambient));
        loc = glGetUniformLocation(shader, "directionalLight.diffuse");
        glUniform3fv(loc, 1, glm::value_ptr(lightSources[i]->diffuse));
        loc = glGetUniformLocation(shader, "directionalLight.specular");
        glUniform3fv(loc, 1, glm::value_ptr(lightSources[i]->specular));
      }
    // camera position
    loc = glGetUniformLocation(shader, "viewPos");
    glUniform3fv(loc, 1, glm::value_ptr(camera->position));
    // draw calls
    glBindVertexArray(filter.vertexArray);
    if (filter.indexCount > 0)
      glDrawElements(GL_TRIANGLES, filter.indexCount, GL_UNSIGNED_INT, 0);
    else
      glDrawArrays(GL_TRIANGLES, 0, filter.vertexCount);
  });
}
