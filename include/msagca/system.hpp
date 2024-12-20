#pragma once
#include <glm/ext/matrix_float4x4.hpp>
#include <shader.hpp>
#include <entity_manager.hpp>
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
  std::unordered_map<GLuint, std::string> shaderIndexNameMap;
  std::unordered_map<std::string, GLuint> shaderNameIndexMap;
  GLuint defaultLit;
  GLuint wireframe;
  bool wireframeMode = false;
public:
  RenderSystem(EntityManager&);
  ~RenderSystem();
  void SetCamera(Camera*);
  GLuint CreateShader(const std::string, const char*, const char*);
  void DeleteShader(const std::string);
  void DeleteShader(GLuint);
  GLuint GetShaderID(const std::string);
  std::string GetShaderName(GLuint);
  void ToggleWireframeMode();
  void Update() override;
};
