#pragma once
#include <concepts.hpp>
#include <kuki_engine_export.h>
#include <typeindex>
#include <utility>
namespace kuki {
class KUKI_ENGINE_API RenderTarget {
public:
  virtual ~RenderTarget() = default;
  template <typename T>
  auto As(this auto &self) -> ConstCorrectPointer<decltype(self), T>;
  template <typename T>
  auto Is() const -> bool;
protected:
  template <typename T>
  RenderTarget(std::in_place_type_t<T>);
private:
  std::type_index typeIndex;
};
template <typename T>
RenderTarget::RenderTarget(std::in_place_type_t<T>)
  : typeIndex(typeid(T)) {}
template <typename T>
auto RenderTarget::As(this auto &self) -> ConstCorrectPointer<decltype(self), T> {
  if (self.template Is<T>())
    return static_cast<ConstCorrectPointer<decltype(self), T>>(&self);
  return nullptr;
}
template <typename T>
auto RenderTarget::Is() const -> bool {
  return typeIndex == typeid(T);
}
} // namespace kuki
