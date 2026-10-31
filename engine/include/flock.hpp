#pragma once
#include <cstdint>
#include <functional>
#include <glm/ext/vector_float3.hpp>
#include <kuki_engine_export.h>
#include <random.hpp>
#include <span>
#include <vector>
namespace kuki {
/// @brief One member of a flock: where it is and how it is moving.
struct Boid {
  glm::vec3 position{};
  glm::vec3 velocity{};
  /// @brief How far through its wingbeat it is, in turns, and how hard it is beating: one climbing
  /// or turning hard, nought gliding. For whatever draws it; the flock does not read either.
  float phase{};
  float effort{};
};
/// @brief How a flock behaves: Reynolds' three rules, and what keeps it in the sky and over the map.
struct FlockSettings {
  /// @brief How far a boid sees its neighbours, and how close it lets one come before it moves away.
  float sightRadius{4.f};
  float personalSpace{1.2f};
  /// @brief How hard it keeps its distance, matches its neighbours' heading and closes on their middle.
  float separation{3.f};
  float alignment{1.2f};
  float cohesion{.8f};
  /// @brief How hard it heads for the flock's goal, which a caller moves to make the flock roam.
  float seek{.6f};
  /// @brief Its speed is kept between these, in units a second. A bird has a speed it cannot fly
  /// below, which is what keeps a flock from bunching to a stop at its goal.
  float minSpeed{4.f};
  float maxSpeed{8.f};
  /// @brief The most it can change its velocity in a second. What makes it bank round a turn
  /// rather than reverse on the spot.
  float maxAccel{10.f};
  /// @brief The height band above the ground it keeps to, and how hard it is pushed back into it.
  float minHeight{6.f};
  float maxHeight{16.f};
  float heightForce{6.f};
  /// @brief How hard a gust carries it, in units a second squared per unit of wind. Nought ignores it.
  float drift{.15f};
  /// @brief Wingbeats a second when beating hardest.
  float flapRate{4.f};
};
/// @brief A flock of birds, a school of fish, a swarm: boids that steer by what their neighbours do.
///
/// Each boid looks at the others within `sightRadius` and steers away from those too close, towards
/// their average heading and towards their middle, then towards the flock's goal; the sum is held to
/// what it can turn in a step and its speed to a band. Nothing leads, and the flock still moves as
/// one, splitting round things and joining again.
///
/// The ground is the caller's: `ground` answers how high it is under a point, and the flock keeps a
/// band of height above whatever that is. Without it the ground is at nought.
///
/// Every neighbour is looked at by every boid, which is the right trade for a few dozen. A few
/// thousand would want the neighbours bucketed.
class KUKI_ENGINE_API Flock {
public:
  Flock() = default;
  /// @brief A flock of `count` boids scattered about a point, heading every way at a cruising speed.
  Flock(const size_t count, const glm::vec3 &centre, const FlockSettings &, const uint64_t seed = 0);
  /// @brief Moves the flock on by `dt` seconds, under a wind of the given velocity.
  auto Step(const float dt, const glm::vec3 &wind = glm::vec3(.0f)) -> void;
  auto SetGoal(const glm::vec3 &) -> void;
  auto GetGoal() const -> const glm::vec3 &;
  auto SetGround(std::function<float(float, float)>) -> void;
  auto GetBoids() const -> std::span<const Boid>;
  auto GetSettings() -> FlockSettings &;
  /// @brief The middle of the flock, and its average velocity.
  auto Centre() const -> glm::vec3;
  auto Heading() const -> glm::vec3;
private:
  std::vector<Boid> boids;
  std::vector<glm::vec3> steering;
  FlockSettings settings;
  glm::vec3 goal{};
  std::function<float(float, float)> ground;
  Random random;
};
} // namespace kuki
