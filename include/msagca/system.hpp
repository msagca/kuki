#pragma once
#include <glm/ext/matrix_float4x4.hpp>
#include <shader.hpp>
#include <entity_manager.hpp>
#include <unordered_set>
#include <camera_controller.hpp>
class ISystem {
public:
  virtual ~ISystem() = default;
  virtual void Update() = 0;
};
class RenderSystem : ISystem {
private:
  EntityManager& entityManager;
  Camera* camera;
  std::unordered_set<GLuint> shaderDB;
  GLuint defaultShader;
public:
  RenderSystem(EntityManager&);
  void SetCamera(Camera*);
  GLuint AddShader(const char*, const char*);
  void RemoveShader(GLuint);
  void Update() override;
};
