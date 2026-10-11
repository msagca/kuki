#pragma once
#include <particle_emitter.hpp>
namespace kuki {
/// @brief A named starting point for an emitter: an effect, as far as the engine has such a thing.
///
/// An effect is data, and this is all of it -- a function that fills in a `ParticleEmitter`. There
/// is deliberately no enumeration of effects for a backend to switch on. A backend implements the
/// two things that are a different piece of shader each, `ParticleShape` and `ParticleBlend`, and
/// every effect is some values for the component that picks one of each. So a new one is written
/// here, or in a game, and no renderer is opened to add it.
///
/// Starting points rather than finished effects. What a spark should weigh depends on how big the
/// scene is, so anything taken from here is expected to be adjusted by whoever took it.
struct ParticlePreset {
  const char *name;
  ParticleEmitter (*make)();
};
namespace ParticlePresets {
/// @brief A steady spray of sparks along the local +Y. The component's own defaults, under a name.
inline auto Fountain() -> ParticleEmitter {
  return {};
}
/// @brief A few hundred sparks thrown wide, once, each time `fired` is raised.
///
/// Slowed hard, so the burst opens quickly and then hangs and falls rather than travelling. The
/// pool is several bursts deep, since two can follow one another inside a lifetime.
inline auto Burst() -> ParticleEmitter {
  ParticleEmitter emitter;
  emitter.capacity = 2048;
  emitter.emitting = false;
  emitter.burst = 320;
  emitter.lifetime = .9f;
  emitter.lifetimeVariance = .6f;
  emitter.speed = 4.2f;
  emitter.speedVariance = .7f;
  emitter.spread = 75.f;
  emitter.radius = .18f;
  emitter.drag = 1.6f;
  emitter.startSize = .06f;
  return emitter;
}
/// @brief Embers: slow, few, and carried upwards by their own heat instead of falling.
inline auto Embers() -> ParticleEmitter {
  ParticleEmitter emitter;
  emitter.capacity = 512;
  emitter.rate = 60.f;
  emitter.lifetime = 4.f;
  emitter.speed = .5f;
  emitter.spread = 60.f;
  emitter.radius = .3f;
  emitter.gravity = {.0f, .5f, .0f};
  emitter.drag = .8f;
  emitter.startSize = .04f;
  emitter.intensity = 4.f;
  return emitter;
}
/// @brief A mesh coming apart: the whole pool born on its surface at once, standing as the mesh for
/// a moment, then let go a grain at a time to lift off along the normals and shrink to nothing.
///
/// Carried upwards, and firmly. Left to drift, the grains hang where the mesh was as a cloud the
/// shape of it for most of their lives, and whatever is put there next is put inside the cloud.
///
/// The mesh has to be named by whoever takes this, and so does the colour, which should be the
/// colour of the thing dissolving. The pool is the burst: every slot has its own point on the
/// surface, so filling all of them is what covers it evenly, and a grain the size given here covers
/// a surface of a few square units without gaps. A bigger mesh wants more of them or bigger ones.
inline auto Dissolve() -> ParticleEmitter {
  ParticleEmitter emitter;
  emitter.capacity = 32768;
  emitter.emitting = false;
  emitter.burst = emitter.capacity;
  emitter.shape = ParticleShape::Mesh;
  emitter.blend = ParticleBlend::Opaque;
  emitter.lifetime = 1.f;
  emitter.lifetimeVariance = .5f;
  emitter.hold = .45f;
  emitter.holdVariance = .8f;
  emitter.speed = 1.3f;
  emitter.speedVariance = .8f;
  emitter.spread = 40.f;
  emitter.gravity = {.0f, 2.4f, .0f};
  emitter.drag = 1.f;
  emitter.startSize = .03f;
  emitter.endSize = .0f;
  emitter.startColor = {.8f, .8f, .8f};
  emitter.endColor = {.8f, .8f, .8f};
  emitter.intensity = 1.f;
  return emitter;
}
} // namespace ParticlePresets
/// @brief Every preset above, for whatever wants to offer them by name. The editor does.
inline constexpr ParticlePreset PARTICLE_PRESETS[]{
  {"Fountain", &ParticlePresets::Fountain},
  {"Burst", &ParticlePresets::Burst},
  {"Embers", &ParticlePresets::Embers},
  {"Dissolve", &ParticlePresets::Dissolve},
};
} // namespace kuki
