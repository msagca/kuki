#pragma once
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
  virtual ~Renderer() = default;
  virtual auto Clear() -> void = 0;
  virtual auto CreateBuffer(const std::string & = "", const int & = 0) -> EntityID = 0;
  virtual auto CreateTarget(const TargetDescription &, const std::string & = "") -> EntityID = 0;
  virtual auto CreateTexture(const TargetDescription &, const std::string & = "") -> EntityID = 0;
  virtual auto GetBuffer(const EntityID) -> BufferObject * = 0;
  virtual auto GetBuffer(const std::string &) -> BufferObject * = 0;
  virtual auto GetCompute(const std::string &) -> Shader * = 0;
  virtual auto GetPrimitive(const std::string &) -> BufferObject * = 0;
  virtual auto GetScene(const std::string & = "") -> Scene * = 0;
  virtual auto GetShader(const MaterialType) -> Shader * = 0;
  virtual auto GetShader(const std::string &, const MaterialType) -> Shader * = 0;
  virtual auto GetTarget(const EntityID) -> RenderTarget * = 0;
  virtual auto GetTarget(const std::string &) -> RenderTarget * = 0;
  virtual auto GetTexture(const EntityID) -> RenderTarget * = 0;
  virtual auto GetTexture(const std::string &) -> RenderTarget * = 0;
  virtual auto LoadAsset(const AssetID) -> void = 0;
  virtual auto LoadScene(Scene &) -> void = 0;
  virtual auto PreviewAsset(const AssetID) -> RenderTarget * = 0;
  virtual auto Reset() -> void = 0;
  virtual auto UpdateTarget(const std::string &, const TargetDescription &) -> void = 0;
  template <typename T>
  auto As(this auto &self) -> ConstCorrectPointer<decltype(self), T>;
  template <typename T>
  auto Is() const -> bool;
protected:
  template <typename T>
  Renderer(std::in_place_type_t<T>);
private:
  std::type_index typeIndex;
};
template <typename T>
Renderer::Renderer(std::in_place_type_t<T>)
  : typeIndex(typeid(T)) {}
template <typename T>
auto Renderer::As(this auto &self) -> ConstCorrectPointer<decltype(self), T> {
  if (self.template Is<T>())
    return static_cast<ConstCorrectPointer<decltype(self), T>>(&self);
  return nullptr;
}
template <typename T>
auto Renderer::Is() const -> bool {
  return typeIndex == typeid(T);
}
} // namespace kuki
