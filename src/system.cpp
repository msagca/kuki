#include <glm/ext/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <system.hpp>
#include <entity.hpp>
static const auto X_AXIS = glm::vec3(1.0f, 0.0f, 0.0f);
static const auto Y_AXIS = glm::vec3(0.0f, 1.0f, 0.0f);
static const auto Z_AXIS = glm::vec3(0.0f, 0.0f, 1.0f);
RenderSystem::RenderSystem(EntityManager& entityManager)
  : transformManager(entityManager.GetManager<Transform>()),
    filterManager(entityManager.GetManager<MeshFilter>()),
    rendererManager(entityManager.GetManager<MeshRenderer>()) {
  defaultShader = AddShader("default.vert", "default.frag");
  projection = glm::perspective(glm::radians(VIEW_ANGLE), static_cast<float>(4) / 3, NEAR_PLANE, FAR_PLANE);
}
GLuint RenderSystem::AddShader(const char* vert, const char* frag) {
  Shader shader(vert, frag);
  shaderDB[shader.ID] = shader;
  return shader.ID;
}
void RenderSystem::RemoveShader(GLuint id) {
  shaderDB.erase(id);
  glDeleteProgram(id);
}
void RenderSystem::SetAspectRatio(float ratio) {
  projection = glm::perspective(glm::radians(VIEW_ANGLE), ratio, NEAR_PLANE, FAR_PLANE);
  for (const auto& pair : shaderDB) {
    glUseProgram(pair.second.ID);
    auto loc = glGetUniformLocation(pair.second.ID, "projection");
    glUniformMatrix4fv(loc, 1, GL_FALSE, glm::value_ptr(projection));
  }
}
void RenderSystem::SetActiveCamera(const Camera* camera) {
  activeCamera = camera;
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
  auto view = glm::mat4(1.0f);
  if (activeCamera != nullptr)
    view = activeCamera->GetViewMatrix();
  for (auto it = rendererManager.begin(); it != rendererManager.end(); it++) {
    auto& renderer = *it;
    auto componentID = std::distance(rendererManager.begin(), it);
    auto entityID = rendererManager.GetEntityID(componentID);
    auto& transform = transformManager.GetComponent(entityID);
    auto& filter = filterManager.GetComponent(entityID);
    auto& shader = shaderDB[defaultShader];
    if (shaderDB.find(renderer.shader) != shaderDB.end())
      shader = shaderDB[renderer.shader];
    glUseProgram(shader.ID);
    // TODO: cache world transforms and update them on change via callbacks
    auto model = GetWorldTransform(transform);
    auto loc = glGetUniformLocation(shader.ID, "model");
    glUniformMatrix4fv(loc, 1, GL_FALSE, glm::value_ptr(model));
    loc = glGetUniformLocation(shader.ID, "view");
    glUniformMatrix4fv(loc, 1, GL_FALSE, glm::value_ptr(view));
    glBindVertexArray(filter.vao);
    if (filter.indexCount > 0)
      glDrawElements(GL_TRIANGLES, filter.indexCount, GL_UNSIGNED_INT, 0);
    else
      glDrawArrays(GL_TRIANGLES, 0, filter.vertexCount);
  }
}
