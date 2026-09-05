#include <spdlog/spdlog.h>
#include <string>
#include <trie.hpp>
namespace kuki {
auto LogActionFired(const std::string &trigger) -> void {
  spdlog::info("[Trie] firing action for trigger {}", trigger);
}
} // namespace kuki
