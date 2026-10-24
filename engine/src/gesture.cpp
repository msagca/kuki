#include <algorithm>
#include <bit>
#include <charconv>
#include <gesture.hpp>
#include <spdlog/spdlog.h>
#include <utility>
namespace kuki {
namespace {
  /// @brief Reads button names joined by `+` into a mask. Empty if one is not a button.
  auto ParseButtons(std::string_view text, const std::span<const std::string> names) -> std::optional<ButtonMask> {
    ButtonMask mask{};
    // Until there is no `+` left rather than until the text runs out, so that the nothing after a
    // trailing `+` is looked up like any other name and refused for not being one.
    for (auto more = true; more;) {
      const auto plus = text.find('+');
      const auto name = text.substr(0, plus);
      more = plus != std::string_view::npos;
      text = more ? text.substr(plus + 1) : std::string_view{};
      const auto it = std::ranges::find(names, name);
      if (it == names.end() || it - names.begin() >= MAX_BUTTONS)
        return std::nullopt;
      mask |= ButtonMask{1} << (it - names.begin());
    }
    return mask;
  }
  auto ParseStep(std::string_view text, const std::span<const std::string> names) -> std::optional<GestureStep> {
    GestureStep step;
    if (const auto slash = text.find('/'); slash != std::string_view::npos) {
      const auto with = ParseButtons(text.substr(0, slash), names);
      if (!with)
        return std::nullopt;
      step.with = *with;
      text = text.substr(slash + 1);
    }
    if (text.starts_with('-')) {
      step.kind = GestureStepKind::Release;
      text = text.substr(1);
    }
    if (const auto star = text.find('*'); star != std::string_view::npos) {
      // A release that is also a hold is not a thing a hand can do, so it is not a thing to guess at.
      if (step.kind == GestureStepKind::Release)
        return std::nullopt;
      const auto digits = text.substr(star + 1);
      const auto [end, error] = std::from_chars(digits.data(), digits.data() + digits.size(), step.ticks);
      if (error != std::errc{} || end != digits.data() + digits.size())
        return std::nullopt;
      step.kind = GestureStepKind::Hold;
      text = text.substr(0, star);
    }
    const auto buttons = ParseButtons(text, names);
    if (!buttons)
      return std::nullopt;
    step.buttons = *buttons;
    return step;
  }
} // namespace
auto ParseGesture(std::string_view text, const std::span<const std::string> names) -> std::optional<std::vector<GestureStep>> {
  std::vector<GestureStep> steps;
  while (true) {
    const auto begin = text.find_first_not_of(" \t");
    if (begin == std::string_view::npos)
      break;
    const auto end = text.find_first_of(" \t", begin);
    const auto step = ParseStep(text.substr(begin, end == std::string_view::npos ? end : end - begin), names);
    if (!step)
      return std::nullopt;
    steps.push_back(*step);
    text = end == std::string_view::npos ? std::string_view{} : text.substr(end);
  }
  return steps.empty() ? std::nullopt : std::optional(std::move(steps));
}
auto GestureRecognizer::Add(Gesture gesture) -> GestureID {
  if (gesture.steps.empty() || gesture.window == 0) {
    spdlog::warn("[GestureRecognizer] Refused {}: it has no steps, or no time to make them in", gesture.name);
    return INVALID_GESTURE;
  }
  for (const auto &step : gesture.steps)
    if (!step.buttons || (step.kind == GestureStepKind::Hold && (step.ticks == 0 || step.ticks >= HISTORY))) {
      spdlog::warn("[GestureRecognizer] Refused {}: a step names no button, or holds for a time that is nought or past the {} ticks kept", gesture.name, HISTORY);
      return INVALID_GESTURE;
    }
  const auto id = static_cast<GestureID>(gestures.size());
  gestures.push_back(std::move(gesture));
  order.push_back(id);
  const auto weight = [this](const GestureID which) {
    auto buttons = 0;
    for (const auto &step : gestures[which].steps)
      buttons += std::popcount(step.buttons | step.with);
    return std::pair(gestures[which].steps.size(), buttons);
  };
  // Stable, so that what is left equal after steps and buttons stays in the order it was added.
  std::ranges::stable_sort(order, [&weight](const GestureID a, const GestureID b) { return weight(a) > weight(b); });
  return id;
}
auto GestureRecognizer::Add(std::string name, const std::string_view notation, const std::span<const std::string> names, const uint16_t window, const bool strict) -> GestureID {
  auto steps = ParseGesture(notation, names);
  if (!steps) {
    spdlog::warn("[GestureRecognizer] Refused {}: \"{}\" does not parse, or names a button that is not registered", name, notation);
    return INVALID_GESTURE;
  }
  return Add(Gesture{std::move(name), std::move(*steps), window, strict});
}
auto GestureRecognizer::Advance(const ButtonMask held) -> std::span<const GestureID> {
  history[count % HISTORY] = held;
  const auto now = count++;
  matched.clear();
  for (const auto id : order)
    if (Match(gestures[id], gestures[id].steps.size() - 1, now))
      matched.push_back(id);
  return matched;
}
auto GestureRecognizer::Get(const GestureID id) const -> const Gesture * {
  return id < gestures.size() ? &gestures[id] : nullptr;
}
auto GestureRecognizer::GetHeld() const -> ButtonMask {
  return At(count - 1);
}
auto GestureRecognizer::GetHeldTicks(const ButtonMask buttons) const -> int {
  return buttons ? Streak(buttons, count - 1) : 0;
}
auto GestureRecognizer::Reset() -> void {
  history.fill(0);
  count = 0;
}
auto GestureRecognizer::GetMemory() const -> Memory {
  return {history, count};
}
auto GestureRecognizer::SetMemory(const Memory &memory) -> void {
  history = memory.history;
  count = memory.count;
  matched.clear();
}
auto GestureRecognizer::At(const int64_t tick) const -> ButtonMask {
  // Nothing was down before the first tick, and nothing is remembered from further back than the
  // ring goes. Answering nought for both is what lets every caller walk backwards without checking.
  if (tick < 0 || tick >= count || tick < count - HISTORY)
    return 0;
  return history[tick % HISTORY];
}
auto GestureRecognizer::Pressed(const int64_t tick) const -> ButtonMask {
  return At(tick) & ~At(tick - 1);
}
auto GestureRecognizer::Released(const int64_t tick) const -> ButtonMask {
  return At(tick - 1) & ~At(tick);
}
auto GestureRecognizer::Streak(const ButtonMask buttons, const int64_t tick) const -> int {
  auto length = 0;
  while (length < HISTORY && (At(tick - length) & buttons) == buttons)
    ++length;
  return length;
}
auto GestureRecognizer::Completes(const Gesture &gesture, const size_t index, const int64_t tick, int64_t &begin) const -> bool {
  const auto &step = gesture.steps[index];
  if ((At(tick) & step.with) != step.with)
    return false;
  begin = tick;
  switch (step.kind) {
  case GestureStepKind::Press: {
    if (!(Pressed(tick) & step.buttons) || (At(tick) & step.buttons) != step.buttons)
      return false;
    // Each key of a chord has to have gone down recently, not merely be down. Otherwise resting a
    // finger on one key would turn every press of the other into the chord.
    ButtonMask arrived{};
    for (auto back = 0; back <= CHORD_WINDOW; ++back)
      arrived |= Pressed(tick - back);
    return (arrived & step.buttons) == step.buttons;
  }
  case GestureStepKind::Hold: {
    const auto streak = Streak(step.buttons, tick);
    begin = tick - streak + 1;
    return index + 1 == gesture.steps.size() ? streak == step.ticks : streak >= step.ticks;
  }
  case GestureStepKind::Release:
    return (Released(tick) & step.buttons) && (At(tick - 1) & step.buttons) == step.buttons;
  }
  return false;
}
auto GestureRecognizer::Match(const Gesture &gesture, const size_t index, const int64_t tick) const -> bool {
  int64_t begin{};
  if (!Completes(gesture, index, tick, begin))
    return false;
  if (index == 0)
    return true;
  const auto &step = gesture.steps[index];
  // Strictly before this step began, never on the same tick. Allowing the same tick would let one
  // press serve as two steps, and a double tap would be made by a single one.
  for (auto earlier = begin - 1; earlier >= begin - gesture.window && earlier >= 0; --earlier) {
    if (Match(gesture, index - 1, earlier))
      return true;
    // Walking backwards, so a stray press found here lies between every earlier candidate and this
    // step as well, and there is no point looking further.
    if (gesture.strict && (Pressed(earlier) & ~(step.buttons | step.with)))
      return false;
  }
  return false;
}
} // namespace kuki
