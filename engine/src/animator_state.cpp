#include <animator.hpp>
#include <animator_state.hpp>
#include <cstdint>
using namespace kuki;
auto kuki::SaveAnimatorTick(StateWriter &writer, const Animator &animator) -> void {
  writer.Write(static_cast<int32_t>(animator.state));
  writer.Write(static_cast<int32_t>(animator.stateTicks));
  writer.Write(static_cast<uint8_t>(animator.finished));
  writer.Write(animator.phase);
  writer.Write(animator.previousPhase);
  writer.Write(animator.rootDelta.x);
  writer.Write(animator.rootDelta.y);
  writer.Write(animator.rootDelta.z);
  writer.Write(static_cast<uint32_t>(animator.events.size()));
  for (const auto &event : animator.events) {
    writer.Write(static_cast<uint32_t>(event.size()));
    for (const auto character : event)
      writer.Write(character);
  }
}
auto kuki::LoadAnimatorTick(StateReader &reader, Animator &animator) -> void {
  animator.state = reader.Read<int32_t>();
  animator.stateTicks = reader.Read<int32_t>();
  animator.finished = reader.Read<uint8_t>() != 0;
  animator.phase = reader.Read<float>();
  animator.previousPhase = reader.Read<float>();
  animator.rootDelta.x = reader.Read<float>();
  animator.rootDelta.y = reader.Read<float>();
  animator.rootDelta.z = reader.Read<float>();
  const auto count = reader.Read<uint32_t>();
  animator.events.clear();
  for (auto index = 0u; index < count && reader.Ok(); ++index) {
    std::string event(reader.Read<uint32_t>(), '\0');
    for (auto &character : event)
      character = reader.Read<char>();
    animator.events.push_back(std::move(event));
  }
}
