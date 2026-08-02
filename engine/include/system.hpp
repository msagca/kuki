#pragma once
#include <concepts.hpp>
#include <kuki_engine_export.h>
#include <scene.hpp>
namespace kuki {
class System {
public:
  virtual ~System() = default;
  virtual auto Start() -> void {};
  virtual auto Update(const float) -> void {};
  virtual auto Shutdown() -> void {};
protected:
  template <typename T>
  explicit System(std::in_place_type_t<T>, Application &);
  template <IsSystem T>
  auto As(this auto &self) -> ConstCorrectPointer<decltype(self), T>;
  template <IsSystem T>
  auto Is() const -> bool;
  Application &app;
private:
  std::type_index typeIndex;
};
template <typename T>
System::System(std::in_place_type_t<T>, Application &app)
  : typeIndex(typeid(T)), app(app) {}
template <IsSystem T>
auto System::As(this auto &self) -> ConstCorrectPointer<decltype(self), T> {
  if (self.template Is<T>())
    return &self;
  return nullptr;
}
template <IsSystem T>
auto System::Is() const -> bool {
  return typeIndex == typeid(T);
}
} // namespace kuki
