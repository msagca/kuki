#pragma once
#include <concepts.hpp>
#include <kuki_engine_export.h>
#include <scene.hpp>
namespace kuki {
class KUKI_ENGINE_API System {
public:
  virtual ~System() = default;
  virtual auto Awake() -> void {};
  virtual auto Start() -> void {};
  virtual auto Update(const float) -> void {};
  virtual auto Shutdown() -> void {};
protected:
  template <typename T>
  explicit System(std::in_place_type_t<T>);
  template <IsSystem T>
  auto As(this auto &) -> decltype(auto);
  template <IsSystem T>
  auto Is() const -> bool;
private:
  std::type_index typeIndex;
};
template <typename T>
System::System(std::in_place_type_t<T>)
  : typeIndex(typeid(T)) {}
template <IsSystem T>
auto System::As(this auto &self) -> decltype(auto) {
  if (self.template Is<T>())
    return static_cast<ConstCorrectPointer<decltype(self), T>>(&self);
  return static_cast<ConstCorrectPointer<decltype(self), T>>(nullptr);
}
template <IsSystem T>
auto System::Is() const -> bool {
  return typeIndex == typeid(T);
}
} // namespace kuki
