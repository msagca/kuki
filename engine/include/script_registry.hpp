#pragma once
#include <application.hpp>
#include <functional>
#include <id.hpp>
#include <kuki_engine_export.h>
#include <script.hpp>
#include <string>
#include <type_traits>
#include <typeindex>
#include <unordered_map>
namespace kuki {
struct ScriptTypeInfo {
  std::string name;
  std::function<void(Application &, const EntityID)> add;
};
class KUKI_ENGINE_API ScriptRegistry {
public:
  template <typename T>
  static auto Register() -> bool;
  static auto GetTypes() -> const std::unordered_map<std::type_index, ScriptTypeInfo> &;
private:
  static auto Types() -> std::unordered_map<std::type_index, ScriptTypeInfo> &;
};
template <typename T>
auto ScriptRegistry::Register() -> bool {
  static_assert(std::is_base_of_v<Script, T> && !std::is_same_v<Script, T>, "ScriptRegistry::Register<T>() requires a concrete Script-derived type");
  auto &types = Types();
  const std::type_index type(typeid(T));
  if (types.contains(type))
    return true;
  const T temp;
  types.emplace(type, ScriptTypeInfo{temp.GetName(), [](Application &app, const EntityID id) { app.AddEntityComponent<T>(id); }});
  return true;
}
#define KUKI_REGISTER_SCRIPT(T) \
  namespace { \
    const bool T##_kuki_script_registered = ::kuki::ScriptRegistry::Register<T>(); \
  }
} // namespace kuki
