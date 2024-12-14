#include <glm/ext/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <system.hpp>
#include <entity_manager.hpp>
static const auto X_AXIS = glm::vec3(1.0f, 0.0f, 0.0f);
static const auto Y_AXIS = glm::vec3(0.0f, 1.0f, 0.0f);
static const auto Z_AXIS = glm::vec3(0.0f, 0.0f, 1.0f);
RenderSystem::RenderSystem(EntityManager& entityManager)
  : entityManager(&entityManager) {
  defaultShader = AddShader("default.vert", "default.frag");
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
  // TODO: if there is no active camera scan the entities for camera components
  this->camera = &camera;
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
  if (!camera)
    return;
  auto begin = entityManager->Begin<Primitive>();
  auto end = entityManager->End<Primitive>();
  // TODO: add render loops for other types or type combinations
  for (auto& it = begin; it != end; it++) {
    // TODO: do instanced rendering for objects sharing the same mesh
    auto& primitive = *it;
    auto shader = defaultShader;
    if (shaderDB.find(primitive.renderer.shader) != shaderDB.end())
      shader = primitive.renderer.shader;
    glUseProgram(shader);
    auto model = GetWorldTransform(primitive.transform);
    auto loc = glGetUniformLocation(shader, "model");
    glUniformMatrix4fv(loc, 1, GL_FALSE, glm::value_ptr(model));
    loc = glGetUniformLocation(shader, "view");
    glUniformMatrix4fv(loc, 1, GL_FALSE, glm::value_ptr(camera->view));
    loc = glGetUniformLocation(shader, "projection");
    glUniformMatrix4fv(loc, 1, GL_FALSE, glm::value_ptr(camera->projection));
    glBindVertexArray(primitive.filter.vao);
    if (primitive.filter.indexCount > 0)
      glDrawElements(GL_TRIANGLES, primitive.filter.indexCount, GL_UNSIGNED_INT, 0);
    else
      glDrawArrays(GL_TRIANGLES, 0, primitive.filter.vertexCount);
  }
}
