#include <component_types.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <system.hpp>
#include <entity_manager.hpp>
static const auto X_AXIS = glm::vec3(1.0f, 0.0f, 0.0f);
static const auto Y_AXIS = glm::vec3(0.0f, 1.0f, 0.0f);
static const auto Z_AXIS = glm::vec3(0.0f, 0.0f, 1.0f);
static const auto MAX_LIGHT_SOURCES = 8;
static const auto WIRE_COLOR = glm::vec3(1.0f);
RenderSystem::RenderSystem(EntityManager& entityManager)
  : entityManager(entityManager) {
  defaultLit = CreateShader("DefaultLit", "default_lit.vert", "default_lit.frag");
  wireframe = CreateShader("Wireframe", "wireframe.vert", "wireframe.frag");
}
RenderSystem::~RenderSystem() {
  for (const auto& [id, _] : shaderIndexNameMap)
    glDeleteProgram(id);
}
GLuint RenderSystem::CreateShader(const std::string name, const char* vert, const char* frag) {
  Shader shader(vert, frag);
  shaderIndexNameMap[shader.ID] = name;
  shaderNameIndexMap[name] = shader.ID;
  return shader.ID;
}
void RenderSystem::DeleteShader(const std::string name) {
  auto id = shaderNameIndexMap[name];
  shaderNameIndexMap.erase(name);
  shaderIndexNameMap.erase(id);
  glDeleteProgram(id);
}
void RenderSystem::DeleteShader(GLuint id) {
  shaderNameIndexMap.erase(shaderIndexNameMap[id]);
  shaderIndexNameMap.erase(id);
  glDeleteProgram(id);
}
GLuint RenderSystem::GetShaderID(const std::string name) {
  if (shaderNameIndexMap.find(name) != shaderNameIndexMap.end())
    return shaderNameIndexMap[name];
  return 0;
}
std::string RenderSystem::GetShaderName(GLuint id) {
  if (shaderIndexNameMap.find(id) != shaderIndexNameMap.end())
    return shaderIndexNameMap[id];
  return "";
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
void RenderSystem::Update() {
  if (!camera) {
    auto cameraPtr = entityManager.GetFirst<Camera>();
    if (cameraPtr)
      SetCamera(cameraPtr);
    if (!camera)
      return;
  }
  if (wireframeMode)
    entityManager.ForEach<Transform, MeshFilter>([&](Transform& transform, MeshFilter& filter) {
      auto shader = wireframe;
      glUseProgram(shader);
      auto model = GetWorldTransform(transform);
      auto loc = glGetUniformLocation(shader, "model");
      glUniformMatrix4fv(loc, 1, GL_FALSE, glm::value_ptr(model));
      loc = glGetUniformLocation(shader, "view");
      glUniformMatrix4fv(loc, 1, GL_FALSE, glm::value_ptr(camera->view));
      loc = glGetUniformLocation(shader, "projection");
      glUniformMatrix4fv(loc, 1, GL_FALSE, glm::value_ptr(camera->projection));
      loc = glGetUniformLocation(shader, "wireColor");
      glUniform3fv(loc, 1, glm::value_ptr(WIRE_COLOR));
      glBindVertexArray(filter.mesh.vertexArray);
      if (filter.mesh.indexCount > 0)
        glDrawElements(GL_TRIANGLES, filter.mesh.indexCount, GL_UNSIGNED_INT, 0);
      else
        glDrawArrays(GL_TRIANGLES, 0, filter.mesh.vertexCount);
    });
  else
    entityManager.ForEach<Transform, MeshFilter, MeshRenderer>([&](Transform& transform, MeshFilter& filter, MeshRenderer& renderer) {
      // TODO: do instanced rendering for objects sharing the same mesh
      auto shader = defaultLit;
      if (shaderIndexNameMap.find(renderer.shader) != shaderIndexNameMap.end())
        shader = renderer.shader;
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
