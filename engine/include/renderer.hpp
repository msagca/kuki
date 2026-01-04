#pragma once
#include <buffer_description.hpp>
#include <buffer_object.hpp>
#include <compute_type.hpp>
#include <material_type.hpp>
#include <primitive_type.hpp>
#include <render_target.hpp>
#include <scene.hpp>
#include <scene_manager.hpp>
#include <shader.hpp>
#include <shader_asset.hpp>
#include <string>
#include <target_description.hpp>
namespace kuki {
class Renderer {
public:
  Renderer(SceneManager &);
  virtual auto CreateBuffer(std::string, const BufferDescription &) -> BufferObject * = 0;
  virtual auto CreateCompute(const ComputeType, const ShaderAsset &) -> Shader * = 0;
  virtual auto CreatePrimitive(const PrimitiveType) -> BufferObject * = 0;
  virtual auto CreateShader(const MaterialType, const ShaderAsset &, const ShaderAsset &) -> Shader * = 0;
  virtual auto CreateTarget(std::string, const TargetDescription &) -> RenderTarget * = 0;
  virtual auto GetActiveScene() -> Scene * = 0;
  virtual auto GetCompute(const ComputeType) -> Shader * = 0;
  virtual auto GetPrimitive(const PrimitiveType) -> BufferObject * = 0;
  virtual auto GetScene(const std::string &) -> Scene * = 0;
  virtual auto GetShader(const MaterialType) -> Shader * = 0;
  virtual auto GetTarget(const std::string &) -> RenderTarget * = 0;
  virtual auto Reset() -> void = 0;
protected:
  SceneManager &sceneManager;
};
} // namespace kuki
