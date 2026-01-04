#pragma once
#include <concepts.hpp>
#include <kuki_engine_export.h>
#include <typeindex>
#include <utility>
namespace kuki {
struct KUKI_ENGINE_API BufferObject {
public:
  virtual ~BufferObject() = default;
  template <typename T>
  auto As(this auto &self) -> ConstCorrectPointer<decltype(self), T>;
  template <typename T>
  auto Is() const -> bool;
protected:
  template <typename T>
  BufferObject(std::in_place_type_t<T>);
private:
  std::type_index typeIndex;
};
template <typename T>
BufferObject::BufferObject(std::in_place_type_t<T>)
  : typeIndex(typeid(T)) {}
template <typename T>
auto BufferObject::As(this auto &self) -> ConstCorrectPointer<decltype(self), T> {
  if (self.template Is<T>())
    return static_cast<ConstCorrectPointer<decltype(self), T>>(&self);
  return nullptr;
}
template <typename T>
auto BufferObject::Is() const -> bool {
  return typeIndex == typeid(T);
}
} // namespace kuki
