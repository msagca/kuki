#include <glm/ext/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <system.hpp>
#include <entity_manager.hpp>
static const auto X_AXIS = glm::vec3(1.0f, 0.0f, 0.0f);
static const auto Y_AXIS = glm::vec3(0.0f, 1.0f, 0.0f);
static const auto Z_AXIS = glm::vec3(0.0f, 0.0f, 1.0f);
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
void RenderSystem::SetCamera(Camera& camera) {
  // TODO: if there is no active camera, scan the entities for camera components
  this->camera = &camera;
}
void RenderSystem::SetLight(Light& light) {
  // TODO: allow multiple light sources
  this->light = &light;
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
  if (!camera || !light)
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
    loc = glGetUniformLocation(shader, "light.vector");
    glUniform4fv(loc, 1, glm::value_ptr(glm::vec4(light->vector, light->type == LightType::Directional ? .0f : 1.0f)));
    loc = glGetUniformLocation(shader, "light.ambient");
    glUniform3fv(loc, 1, glm::value_ptr(light->ambient));
    loc = glGetUniformLocation(shader, "light.diffuse");
    glUniform3fv(loc, 1, glm::value_ptr(light->diffuse));
    loc = glGetUniformLocation(shader, "light.specular");
    glUniform3fv(loc, 1, glm::value_ptr(light->specular));
    loc = glGetUniformLocation(shader, "light.constant");
    glUniform1f(loc, light->constant);
    loc = glGetUniformLocation(shader, "light.linear");
    glUniform1f(loc, light->linear);
    loc = glGetUniformLocation(shader, "light.quadratic");
    glUniform1f(loc, light->quadratic);
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
