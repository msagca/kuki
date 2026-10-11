#include <animator.hpp>
namespace kuki {
auto Animator::Play(const std::string_view name, const bool restart) -> bool {
  const auto index = FindState(name);
  if (index < 0)
    return false;
  if (index != state || restart)
    Enter(index);
  return true;
}
auto Animator::Fired(const std::string_view name) const -> bool {
  for (const auto &event : events)
    if (event == name)
      return true;
  return false;
}
auto Animator::FindState(const std::string_view name) const -> int {
  if (name.empty())
    return -1;
  for (size_t i = 0; i < states.size(); ++i)
    if (states[i].name == name)
      return static_cast<int>(i);
  return -1;
}
auto Animator::GetStateName() const -> std::string_view {
  return state >= 0 && state < static_cast<int>(states.size()) ? std::string_view(states[state].name) : std::string_view{};
}
auto Animator::Enter(const int index) -> void {
  if (index < 0 || index >= static_cast<int>(states.size()))
    return;
  state = index;
  stateTicks = 0;
  finished = false;
  // Nothing has been drawn yet the first time, and there is nothing to fade out of.
  fadeFrom = pose;
  fadeTicks = pose.empty() ? 0 : states[index].fade;
}
} // namespace kuki
