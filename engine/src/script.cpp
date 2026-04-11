#include <script.hpp>
namespace kuki {
auto Script::Display() const -> void {}
auto Script::Start(Application &) -> void {}
auto Script::Update(Application &) -> void {}
auto Script::Shutdown(Application &) -> void {}
} // namespace kuki
