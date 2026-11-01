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
  // What fired is a state and a mask of its events, not their names: the names are the controller's.
  writer.Write(static_cast<int32_t>(animator.firedState));
  writer.Write(animator.fired);
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
  animator.firedState = reader.Read<int32_t>();
  animator.fired = reader.Read<uint32_t>();
}
