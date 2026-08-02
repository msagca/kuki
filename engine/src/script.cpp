#include <script.hpp>
#include <string_view>
namespace kuki {
auto Script::Display() const -> void {}
auto Script::GetName() const -> std::string {
  std::string name = typeIndex.name();
  for (const std::string_view prefix : {"class ", "struct "})
    if (name.starts_with(prefix)) {
      name.erase(0, prefix.size());
      break;
    }
  return name;
}
auto Script::GetTypeIndex() const -> std::type_index {
  return typeIndex;
}
auto Script::Start(Application &) -> void {}
auto Script::Update(Application &) -> void {}
auto Script::Shutdown(Application &) -> void {}
} // namespace kuki
