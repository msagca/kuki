#pragma once
#include <cstdint>
#include <glm/ext/vector_float3.hpp>
#include <id.hpp>
namespace kuki {
/// @brief Where an emitter's particles are born.
///
/// One of the two things about an emitter that a backend has to implement rather than merely be
/// told, which is why it is an enumeration and not a number: each value is a different piece of
/// shader. An effect is not one of these. An effect is a set of values for the component below --
/// see `particle_presets.hpp` -- and a new one costs no backend anything.
enum class ParticleShape : uint8_t {
  /// @brief Inside a sphere of `radius` around the emitter, thrown into a cone around its local +Y.
  Sphere,
  /// @brief On the surface of `mesh`, placed by the entity's transform, thrown into a cone around
  /// the surface's own normal at the point each one starts from.
  Mesh,
  /// @brief Anywhere inside a box of half-size `extent` about the emitter, turned and scaled by its
  /// transform, and thrown into the cone around its local +Y.
  ///
  /// What weather is: rain over a whole stage, snow falling everywhere at once, splashes over a whole
  /// floor -- a box with no height. A sphere big enough to cover a stage is mostly air above and
  /// below it, and spawns there are spawns nobody sees.
  Box
};
/// @brief How an emitter's particles are put onto the picture. The other thing a backend implements.
enum class ParticleBlend : uint8_t {
  /// @brief Added to what is behind. Needs no sort and can only brighten: sparks, fire, light.
  Additive,
  /// @brief Solid discs that write depth. Needs no sort either, for the opposite reason -- the depth
  /// buffer settles which of two is in front -- and is the one way to draw a dark particle.
  Opaque,
  /// @brief Laid over what is behind as a thin veil, by the particle's own coverage. Needs a sort to
  /// be exact and gets none: the particles this is for -- mist, steam, dust -- are faint enough
  /// that which of two was laid first cannot be seen. Writes no depth.
  Alpha
};
/// @brief Which way a particle's quad is turned.
enum class ParticleFacing : uint8_t {
  /// @brief Square on to the camera. Sparks, embers, snow.
  Camera,
  /// @brief Stretched along the way it is moving and turned about that line to face the camera as
  /// nearly as it can, `stretch` seconds of its own travel long. A raindrop seen by a camera is a
  /// streak, not a dot, and this is the streak.
  Velocity,
  /// @brief Lying flat, face up. A ring spreading on a puddle.
  Flat
};
/// @brief What is drawn on a particle's quad.
enum class ParticleSprite : uint8_t {
  /// @brief A disc bright at the centre and nothing at its edge.
  Soft,
  /// @brief A thin ring, its width a share of its radius. With `Flat`, a ripple.
  Ring,
  /// @brief A small oval turning end over end at a rate of its own, which is what a petal or a leaf
  /// does on its way down.
  Petal
};
/// @brief A source of particles: sparks, embers, dust -- small bright things there are a lot of.
///
/// Everything a particle does after this component has been read happens on the GPU. The particles
/// live in a buffer the backend owns, a compute shader moves them, and a vertex shader turns each
/// one into a quad facing the camera. Nothing is read back, so the processor never learns where a
/// single particle is, and the cost of an emitter on this side is one small block of constants a
/// frame however many particles it holds.
///
/// That shapes what is here. Every field describes the emitter rather than a particle, because a
/// particle is not something this side of the bus can be asked about: a spawn is a range of slots
/// the shader is told to refill, and what goes into each slot is drawn from a hash of its index.
///
/// The pool is a ring. `capacity` slots, refilled in order at `rate` a second, so a slot is handed
/// to a new particle when the ring comes back round to it -- whether or not the one in it had
/// finished. Size the pool to at least `rate` times the longest lifetime and that never happens;
/// size it smaller and the oldest particles are cut short, which is the right thing to lose. The
/// alternative is a list of free slots kept on the GPU, which buys exactness about who dies and
/// costs an atomic counter in every spawn.
///
/// Drawn additively or as solid discs, and never as anything in between, which is what lets there
/// be no sort: adding is commutative and the depth buffer orders the solid ones. It is also why this
/// suits fire and dust and not smoke. A particle here either brightens what is behind it or hides it.
///
/// Emits along the entity's local +Y, from the entity's position, in world space -- or from the
/// surface of a mesh, along its normals; see `ParticleShape`. A particle that has left the emitter
/// no longer follows it, which is what makes a moving emitter leave a trail.
///
/// Not a scene singleton -- a scene has as many of these as it has things on fire -- and serialized,
/// because where the sparks come from is part of the work.
struct ParticleEmitter {
  /// @brief Slots in the pool, which is the most particles this emitter can have alive at once.
  ///
  /// Also what the emitter costs: every slot is simulated and submitted for drawing in every frame
  /// whether or not anything is alive in it, because which slots are alive is decided on the GPU
  /// and the processor is never told. A dead slot is thirty-two bytes read by a compute thread and
  /// four vertices collapsed onto one point, so the price is small, but it is not nothing and it
  /// is paid for the whole pool.
  ///
  /// Changing it discards the particles in flight, since they live in a buffer of the old size.
  uint32_t capacity{4096};
  /// @brief Particles spawned a second while `emitting` is set.
  float rate{600.f};
  /// @brief Whether new particles are being spawned. The ones already in flight are unaffected.
  ///
  /// Separate from a rate of zero so that a game can stop and start an emitter without having to
  /// remember what the rate was.
  bool emitting{true};
  /// @brief Particles spawned at once, in a single frame, each time `fired` is raised.
  ///
  /// On top of whatever the rate is spawning, and whether or not `emitting` is set: a burst is an
  /// event and the rate is a state, and an emitter that only ever bursts has no use for the other.
  /// No more than `capacity`, which is a burst that refills the whole pool.
  uint32_t burst{};
  /// @brief How many bursts have been asked for. Add one to fire another.
  ///
  /// A count rather than a flag because the renderer reads this component and does not write it, so
  /// there is nobody to clear a flag once the burst has gone off. Each backend remembers the count it
  /// last saw and fires when the two differ. An emitter seen for the first time takes the count as
  /// it finds it, so a scene does not go off like a firework on loading.
  uint32_t fired{};
  /// @brief Where the particles are born. See `ParticleShape`.
  ParticleShape shape{ParticleShape::Sphere};
  /// @brief The mesh whose surface they are born on, when `shape` asks for one.
  ///
  /// Named here rather than taken from a `MeshHandle` beside this component, because an entity with
  /// a mesh handle is drawn -- and the commonest use of this shape is something turning to dust,
  /// where the mesh is exactly what is no longer wanted on screen. To have both, name the same mesh
  /// in both places. The mesh is read and never altered: the backend scatters `capacity` points over
  /// it once, weighted by area, and each slot of the pool is born at its own point from then on.
  ///
  /// A name that resolves to nothing falls back to the sphere.
  AssetID mesh{};
  /// @brief How long a particle lasts, in seconds.
  float lifetime{1.6f};
  /// @brief How much of that a particle may fall short by, as a fraction. Zero makes them all equal.
  ///
  /// Only ever shortens, so `lifetime` stays the longest a particle can live and the sum that sizes
  /// the pool stays one multiplication.
  float lifetimeVariance{.5f};
  /// @brief How long a particle waits where it was born before it starts to move, in seconds.
  ///
  /// Held, a particle is drawn at its starting size and colour and neither ages nor falls. It is
  /// what lets a burst on a mesh stand as the mesh for a moment before it comes apart, and with the
  /// variance below it is what makes that happen a piece at a time rather than all at once.
  ///
  /// On top of `lifetime`, not out of it: a pool that must never cut a particle short is sized by
  /// `rate` times the two together.
  float hold{};
  /// @brief How much of that a particle may fall short by, as a fraction.
  float holdVariance{};
  /// @brief How fast a particle leaves the emitter, in scene units a second.
  float speed{2.4f};
  /// @brief How much of that a particle may fall short by, as a fraction.
  float speedVariance{.5f};
  /// @brief Half-angle of the cone particles are thrown into, in degrees, around the local +Y -- or
  /// around the surface normal, for a particle born on a mesh.
  ///
  /// Nought is a jet, ninety a hemisphere and a hundred and eighty every direction at once.
  float spread{24.f};
  /// @brief Radius of the sphere around the emitter that particles appear inside, in scene units.
  /// Not read by a particle born on a mesh.
  float radius{.05f};
  /// @brief Half the size of the box particles appear in, along the emitter's own axes, when `shape`
  /// asks for a box.
  glm::vec3 extent{1.f, 1.f, 1.f};
  /// @brief Acceleration applied to every particle, in scene units a second squared.
  ///
  /// Gravity by default and by name, but it is any constant acceleration: point it upwards and a
  /// little weaker for embers carried by their own heat.
  glm::vec3 gravity{.0f, -9.81f, .0f};
  /// @brief How quickly the air takes a particle's speed away, per second.
  ///
  /// What keeps a burst from reading as a fountain. Without it every particle follows a clean
  /// parabola, and a spray of clean parabolas is water.
  float drag{.6f};
  /// @brief How hard a gust pushes each particle about, in scene units a second squared.
  ///
  /// Every particle sways to its own rhythm, two slow waves across the floor's two axes with a phase
  /// drawn from its slot, so snow drifts and petals wander rather than falling in lines. Nought is
  /// still air.
  float turbulence{};
  /// @brief Width of a particle's quad when it is spawned, in scene units.
  float startSize{.07f};
  /// @brief Width when it dies. The size in between is a straight line from one to the other.
  float endSize{.0f};
  /// @brief Colour at spawn, and the colour it has faded to by the time it dies.
  glm::vec3 startColor{1.f, .72f, .3f};
  glm::vec3 endColor{1.f, .16f, .02f};
  /// @brief How the particles are put onto the picture. See `ParticleBlend`.
  ///
  /// An opaque particle born on a mesh is shaded once, at birth, by the scene's sun on the normal it
  /// was born on, and keeps that shade. That is a long way short of what the mesh itself is lit by
  /// -- no shadow, no sky, no highlight -- and is there so that a mesh replaced by its own particles
  /// keeps a lit side and a dark side rather than going flat in the frame it is swapped. An opaque
  /// particle from a sphere has no normal and is drawn in its colour as given.
  ParticleBlend blend{ParticleBlend::Additive};
  /// @brief Which way each quad is turned. See `ParticleFacing`.
  ParticleFacing facing{ParticleFacing::Camera};
  /// @brief What each quad carries. See `ParticleSprite`.
  ParticleSprite sprite{ParticleSprite::Soft};
  /// @brief How many seconds of a particle's own travel its streak is long, when it faces along
  /// its velocity. A raindrop is drawn at about a sixtieth: what a camera's shutter would smear it
  /// over.
  float stretch{.02f};
  /// @brief How much of the alpha kind a particle covers at its thickest, from nought to one.
  float opacity{1.f};
  /// @brief What the colours above are multiplied by before they reach the picture.
  ///
  /// Apart from them so that a colour can be picked as a colour. The scene is rendered in linear
  /// light ahead of the bloom chain, and a spark only blooms if it is brighter than white -- which
  /// is a statement about how much light there is, not about what colour it is.
  float intensity{6.f};
  auto operator==(const ParticleEmitter &) const -> bool = default;
};
} // namespace kuki
