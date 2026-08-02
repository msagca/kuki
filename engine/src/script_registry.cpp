#include <script_registry.hpp>
namespace kuki {
auto ScriptRegistry::Types() -> std::unordered_map<std::type_index, ScriptTypeInfo> & {
  static std::unordered_map<std::type_index, ScriptTypeInfo> types;
  return types;
}
auto ScriptRegistry::GetTypes() -> const std::unordered_map<std::type_index, ScriptTypeInfo> & {
  return Types();
}
} // namespace kuki
