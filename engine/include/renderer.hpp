#pragma once
#include <buffer_object.hpp>
#include <material_type.hpp>
#include <render_graph.hpp>
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
  virtual auto ApplyAntiAliasing(std::span<std::string>, std::span<std::string>) -> void = 0;
  virtual auto ApplyBloomEffect(std::span<std::string>, std::span<std::string>) -> void = 0;
  virtual auto ApplyBlurEffect(std::span<std::string>, std::span<std::string>) -> void = 0;
  virtual auto ApplyBrightPassFilter(std::span<std::string>, std::span<std::string>) -> void = 0;
  virtual auto ApplyGammaCorrection(std::span<std::string>, std::span<std::string>) -> void = 0;
  virtual auto Clear() -> void = 0;
  virtual auto CreateShadowMap(std::span<std::string>, std::span<std::string>) -> void = 0;
  virtual auto CreateSpotShadowMap(std::span<std::string>, std::span<std::string>) -> void = 0;
  virtual auto CreateTarget(const TargetDescription &, const std::string & = "") -> EntityID = 0;
  virtual auto GetPreviewSize() const -> int = 0;
  virtual auto GetTarget(const std::string &) -> RenderTarget * = 0;
  virtual auto LoadAsset(const AssetID) -> void = 0;
  virtual auto LoadAssets(const AssetType) -> void = 0;
  virtual auto ApplyOutline(std::span<std::string>, std::span<std::string>) -> void = 0;
  virtual auto LoadScene(Scene &) -> void = 0;
  virtual auto PickEntity(const int, const int) -> EntityID = 0;
  virtual auto PreviewAsset(const AssetID) -> RenderTarget * = 0;
  virtual auto SetPreviewSize(const int) -> void = 0;
  virtual auto RenderScene(std::span<std::string>, std::span<std::string>) -> void = 0;
  virtual auto Reset() -> void = 0;
  virtual auto SetResolution(const int = 1920, const int = 1080) -> void = 0;
  virtual auto UpdateTarget(const std::string &, const TargetDescription &) -> void = 0;
  auto ExecutePass(const RenderPass, std::span<std::string>, std::span<std::string>) -> void;
  template <typename T>
  auto As(this auto &self) -> ConstCorrectPointer<decltype(self), T>;
  template <typename T>
  auto Is() const -> bool;
protected:
  template <typename T>
  Renderer(std::in_place_type_t<T>, Application &);
  Application &app;
private:
  std::type_index typeIndex;
};
template <typename T>
Renderer::Renderer(std::in_place_type_t<T>, Application &app)
  : typeIndex(typeid(T)), app(app) {}
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
