#include <script_registry.hpp>
namespace kuki {
auto ScriptRegistry::Types() -> std::unordered_map<std::type_index, ScriptTypeInfo> & {
  static std::unordered_map<std::type_index, ScriptTypeInfo> types;
  return types;
}
auto ScriptRegistry::GetTypes() -> const std::unordered_map<std::type_index, ScriptTypeInfo> & {
  return Types();
}
auto ScriptRegistry::FindByName(const std::string &name) -> const ScriptTypeInfo * {
  for (const auto &[type, info] : Types())
    if (info.name == name)
      return &info;
  return nullptr;
}
} // namespace kuki
