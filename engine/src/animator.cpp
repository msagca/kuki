#include <animator.hpp>
#include <animator_controller.hpp>
#include <string_view>
namespace kuki {
auto Enter(Animator &animator, const AnimatorController &controller, const int index) -> void {
  if (index < 0 || index >= static_cast<int>(controller.states.size()))
    return;
  animator.state = index;
  animator.stateTicks = 0;
  animator.finished = false;
  animator.phase = .0f;
  animator.previousPhase = .0f;
  // The pose to fade out of is taken when the next frame is drawn, which sees the count move on.
  ++animator.entries;
  animator.fadeTicks = controller.states[index].fade;
}
auto Play(Animator &animator, const AnimatorController &controller, const std::string_view name, const bool restart) -> bool {
  const auto index = controller.FindState(name);
  if (index < 0)
    return false;
  if (index != animator.state || restart)
    Enter(animator, controller, index);
  return true;
}
auto Fired(const Animator &animator, const AnimatorController &controller, const std::string_view name) -> bool {
  if (!animator.fired || animator.firedState < 0 || animator.firedState >= static_cast<int>(controller.states.size()))
    return false;
  const auto &events = controller.states[animator.firedState].events;
  for (size_t event = 0; event < events.size() && event < 32; ++event)
    if ((animator.fired >> event & 1u) && events[event].name == name)
      return true;
  return false;
}
auto GetStateName(const Animator &animator, const AnimatorController &controller) -> std::string_view {
  return animator.state >= 0 && animator.state < static_cast<int>(controller.states.size()) ? std::string_view(controller.states[animator.state].name) : std::string_view{};
}
auto FindGoal(Animator &animator, const std::string_view name) -> IKGoal * {
  for (auto &goal : animator.ik)
    if (goal.name == name)
      return &goal;
  return nullptr;
}
} // namespace kuki
