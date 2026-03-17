#pragma once
#include <buffer_description.hpp>
#include <buffer_object.hpp>
#include <material_type.hpp>
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
  virtual auto CreateTarget(std::string, const TargetDescription &) -> RenderTarget * = 0;
  virtual auto GetActiveScene() -> Scene * = 0;
  virtual auto GetBuffer(const std::string &) -> BufferObject * = 0;
  virtual auto GetCompute(const std::string &) -> Shader * = 0;
  virtual auto GetPrimitive(const std::string &) -> BufferObject * = 0;
  virtual auto GetScene(const std::string &) -> Scene * = 0;
  virtual auto GetShader(const MaterialType) -> Shader * = 0;
  virtual auto GetShader(const std::string &, const MaterialType) -> Shader * = 0;
  virtual auto GetTarget(const std::string &) -> RenderTarget * = 0;
  virtual auto LoadCompute(ShaderAsset &) -> Shader * = 0;
  virtual auto LoadPrimitive(const std::string &) -> BufferObject * = 0;
  virtual auto LoadShader(ShaderAsset &, ShaderAsset &) -> Shader * = 0;
  virtual auto PrepareScene(Scene &) -> void = 0;
  virtual auto Clear() -> void = 0;
  virtual auto Reset() -> void = 0;
protected:
  SceneManager &sceneManager;
};
} // namespace kuki
