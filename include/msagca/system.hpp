#pragma once
#include <glm/ext/matrix_float4x4.hpp>
#include <shader.hpp>
#include <entity_manager.hpp>
#include <unordered_set>
class ISystem {
public:
  virtual ~ISystem() = default;
  virtual void Update() = 0;
};
class RenderSystem : ISystem {
private:
  EntityManager* entityManager;
  Camera* camera;
  Light* light;
  std::unordered_set<GLuint> shaderDB;
  GLuint defaultShader;
public:
  RenderSystem(EntityManager&);
  void SetCamera(Camera&);
  void SetLight(Light&);
  GLuint AddShader(const char*, const char*);
  void RemoveShader(GLuint);
  void Update() override;
};
