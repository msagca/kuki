#pragma once
#include <kuki_engine_export.h>
#include <rollback.hpp>
namespace kuki {
struct Animator;
/// @brief Writes down the part of an animator that a tick decides, to be put back by `LoadAnimatorTick`.
///
/// That is the state it is in, how long it has been there, whether its clip has run out, how far
/// through a blend's cycle it is, and what the last tick fired and travelled -- the facts a game's
/// rules read, and nothing else. The pose is not among them. It is drawn from these and from the
/// fade, and a fade begun from a pose that was showing is not something to take back: put back, the
/// animator is in the right state and shows it from the next frame, which is all a rollback needs
/// of it.
///
/// The states themselves, with their clips and events, are set up once and not written.
KUKI_ENGINE_API auto SaveAnimatorTick(StateWriter &, const Animator &) -> void;
KUKI_ENGINE_API auto LoadAnimatorTick(StateReader &, Animator &) -> void;
} // namespace kuki
