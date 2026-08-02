#pragma once
#include <concepts.hpp>
#include <id.hpp>
#include <kuki_engine_export.h>
#include <script_state.hpp>
#include <string>
#include <typeindex>
namespace kuki {
class Application;
class EntityManager;
class ScriptingSystem;
class KUKI_ENGINE_API Script {
public:
  virtual ~Script() = default;
  auto operator=(const Script &other) -> Script & {
    typeIndex = other.typeIndex;
    return *this;
  }
  EntityID entityId{};
  virtual auto CloneTo(EntityManager &, const EntityID) const -> void = 0;
  virtual auto Display() const -> void;
  virtual auto GetName() const -> std::string;
  virtual auto Start(Application &) -> void;
  virtual auto Update(Application &) -> void;
  virtual auto Shutdown(Application &) -> void;
  template <typename T>
  auto As(this auto &self) -> ConstCorrectPointer<decltype(self), T>;
  auto GetTypeIndex() const -> std::type_index;
  template <typename T>
  auto Is() const -> bool;
protected:
  template <typename T>
  explicit Script(std::in_place_type_t<T>);
private:
  friend class ScriptingSystem;
  std::type_index typeIndex;
  ScriptState state{ScriptState::Idle};
};
template <typename T>
Script::Script(std::in_place_type_t<T>)
  : typeIndex(typeid(T)) {}
template <typename T>
auto Script::As(this auto &self) -> ConstCorrectPointer<decltype(self), T> {
  if (self.template Is<T>())
    return static_cast<ConstCorrectPointer<decltype(self), T>>(&self);
  return nullptr;
}
template <typename T>
auto Script::Is() const -> bool {
  return typeid(T) == typeIndex;
}
} // namespace kuki
