#pragma once
#include <glm/ext/matrix_float4x4.hpp>
#include <camera.hpp>
#include <shader.hpp>
#include <entity.hpp>
static const auto FAR_PLANE = 100.0f;
static const auto NEAR_PLANE = .1f;
static const auto VIEW_ANGLE = 45.0f;
class System {
public:
  virtual ~System() = default;
  virtual void Update() = 0;
};
class RenderSystem : public System {
private:
  const Camera* activeCamera;
  glm::mat4 projection;
  ComponentManager<Transform>& transformManager;
  ComponentManager<MeshFilter>& filterManager;
  ComponentManager<MeshRenderer>& rendererManager;
  std::unordered_map<GLuint, Shader> shaderDB;
  GLuint defaultShader;
public:
  RenderSystem(EntityManager&);
  GLuint AddShader(const char*, const char*);
  void RemoveShader(GLuint);
  void SetAspectRatio(float ratio);
  void SetActiveCamera(const Camera*);
  void Update() override;
};
