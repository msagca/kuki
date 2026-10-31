#include <algorithm>
#include <cmath>
#include <flock.hpp>
#include <glm/geometric.hpp>
namespace kuki {
namespace {
  /// @brief A vector cut to a length if it is longer.
  auto Limit(const glm::vec3 &v, const float length) -> glm::vec3 {
    const auto l = glm::length(v);
    return l > length && l > .0f ? v * (length / l) : v;
  }
} // namespace
Flock::Flock(const size_t count, const glm::vec3 &centre, const FlockSettings &settings, const uint64_t seed)
  : boids(count), steering(count), settings(settings), goal(centre), random(seed, 0xbb67ae8584caa73bull) {
  const auto cruise = (settings.minSpeed + settings.maxSpeed) * .5f;
  for (auto &boid : boids) {
    const auto angle = random.Float() * 6.2831853f;
    const auto reach = std::sqrt(random.Float()) * settings.sightRadius;
    boid.position = centre + glm::vec3(std::cos(angle) * reach, random.Range(-1.f, 1.f), std::sin(angle) * reach);
    const auto heading = random.Float() * 6.2831853f;
    boid.velocity = glm::vec3(std::cos(heading), .0f, std::sin(heading)) * cruise;
    boid.phase = random.Float();
  }
}
auto Flock::Step(const float dt, const glm::vec3 &wind) -> void {
  if (dt <= .0f)
    return;
  const auto sight2 = settings.sightRadius * settings.sightRadius;
  const auto space2 = settings.personalSpace * settings.personalSpace;
  for (size_t i = 0; i < boids.size(); ++i) {
    const auto &boid = boids[i];
    glm::vec3 apart{};
    glm::vec3 heading{};
    glm::vec3 middle{};
    auto seen = 0;
    for (size_t j = 0; j < boids.size(); ++j) {
      if (i == j)
        continue;
      const auto offset = boids[j].position - boid.position;
      const auto distance2 = glm::dot(offset, offset);
      if (distance2 > sight2)
        continue;
      ++seen;
      heading += boids[j].velocity;
      middle += boids[j].position;
      // Harder the closer, so two about to touch part at once and two at the edge of each other's
      // space barely notice.
      if (distance2 < space2 && distance2 > 1e-6f)
        apart -= offset / distance2;
    }
    glm::vec3 force{};
    if (seen > 0) {
      force += apart * settings.separation;
      force += (heading / static_cast<float>(seen) - boid.velocity) * settings.alignment * .5f;
      force += (middle / static_cast<float>(seen) - boid.position) * settings.cohesion * .5f;
    }
    const auto toGoal = goal - boid.position;
    if (const auto length = glm::length(toGoal); length > 1e-3f)
      force += (toGoal / length * settings.maxSpeed - boid.velocity) * settings.seek * .5f;
    const auto floor = ground ? ground(boid.position.x, boid.position.z) : .0f;
    const auto height = boid.position.y - floor;
    if (height < settings.minHeight)
      force.y += (settings.minHeight - height) * settings.heightForce;
    else if (height > settings.maxHeight)
      force.y -= (height - settings.maxHeight) * settings.heightForce;
    force += wind * settings.drift;
    steering[i] = Limit(force, settings.maxAccel);
  }
  for (size_t i = 0; i < boids.size(); ++i) {
    auto &boid = boids[i];
    const auto before = boid.velocity;
    boid.velocity += steering[i] * dt;
    const auto speed = glm::length(boid.velocity);
    if (speed > settings.maxSpeed)
      boid.velocity *= settings.maxSpeed / speed;
    else if (speed < settings.minSpeed && speed > 1e-4f)
      boid.velocity *= settings.minSpeed / speed;
    boid.position += boid.velocity * dt;
    // Beating hard when climbing or turning sharply, gliding when level or coming down: what the
    // eye reads as a bird at work rather than one wound up.
    const auto climb = std::clamp(boid.velocity.y / std::max(settings.maxSpeed, 1e-3f) * 2.f, -1.f, 1.f);
    const auto turn = glm::length(boid.velocity - before) / std::max(settings.maxAccel * dt, 1e-4f);
    const auto wanted = std::clamp(.35f + climb + turn * .4f, .0f, 1.f);
    boid.effort += (wanted - boid.effort) * std::min(1.f, dt * 2.f);
    // Still turning over slowly when gliding, so a wing held out comes back into the beat where it was.
    boid.phase = std::fmod(boid.phase + dt * settings.flapRate * (.15f + .85f * boid.effort), 1.f);
  }
}
auto Flock::SetGoal(const glm::vec3 &value) -> void {
  goal = value;
}
auto Flock::GetGoal() const -> const glm::vec3 & {
  return goal;
}
auto Flock::SetGround(std::function<float(float, float)> function) -> void {
  ground = std::move(function);
}
auto Flock::GetBoids() const -> std::span<const Boid> {
  return boids;
}
auto Flock::GetSettings() -> FlockSettings & {
  return settings;
}
auto Flock::Centre() const -> glm::vec3 {
  glm::vec3 sum{};
  for (const auto &boid : boids)
    sum += boid.position;
  return boids.empty() ? sum : sum / static_cast<float>(boids.size());
}
auto Flock::Heading() const -> glm::vec3 {
  glm::vec3 sum{};
  for (const auto &boid : boids)
    sum += boid.velocity;
  return boids.empty() ? sum : sum / static_cast<float>(boids.size());
}
} // namespace kuki
