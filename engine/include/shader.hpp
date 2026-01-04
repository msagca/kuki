#pragma once
#include <concepts.hpp>
#include <id.hpp>
#include <kuki_engine_export.h>
#include <typeindex>
namespace kuki {
class KUKI_ENGINE_API Shader {
public:
  virtual ~Shader() = default;
  AssetID assetId;
  template <typename T>
  auto As(this auto &self) -> ConstCorrectPointer<decltype(self), T>;
  template <typename T>
  auto Is() const -> bool;
protected:
  template <typename T>
  explicit Shader(std::in_place_type_t<T>);
private:
  std::type_index typeIndex;
};
template <typename T>
Shader::Shader(std::in_place_type_t<T>)
  : typeIndex(typeid(T)) {}
template <typename T>
auto Shader::As(this auto &self) -> ConstCorrectPointer<decltype(self), T> {
  if (self.template Is<T>())
    return static_cast<ConstCorrectPointer<decltype(self), T>>(&self);
  return nullptr;
}
template <typename T>
auto Shader::Is() const -> bool {
  return typeIndex == typeid(T);
}
} // namespace kuki
