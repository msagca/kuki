#include <algorithm>
#include <application.hpp>
#include <cmath>
#include <flock_system.hpp>
#include <frontier.hpp>
#include <frontier_world.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/quaternion.hpp>
#include <map_system.hpp>
#include <material_handle.hpp>
#include <props.hpp>
#include <scene.hpp>
#include <transform.hpp>
#include <wind.hpp>
using namespace kuki;
using namespace frontier;
namespace {
constexpr glm::vec3 UP{.0f, 1.f, .0f};
auto FindMap(Application &app) -> MapSystem * {
  auto *frontier = dynamic_cast<Frontier *>(&app);
  return frontier ? frontier->GetSystem<MapSystem>() : nullptr;
}
} // namespace
FlockSystem::FlockSystem(Application &app)
  : System(std::in_place_type<FlockSystem>, app) {}
auto FlockSystem::Release() -> void {
  for (const auto &birds : flocks)
    for (const auto *list : {&birds.bodies, &birds.left, &birds.right})
      for (const auto id : *list)
        app.DeleteEntity(id);
  flocks.clear();
}
auto FlockSystem::PickGoal(Birds &birds) -> void {
  auto *mapSystem = FindMap(app);
  if (!mapSystem)
    return;
  const auto &map = mapSystem->GetMap();
  // A few tries at a cell of the right kind -- a shore for gulls, open land for swallows -- and the
  // last try kept whatever it is, so a map with no shore still has somewhere to fly.
  CellIndex goal = INVALID_CELL;
  for (auto attempt = 0; attempt < 40; ++attempt) {
    const auto cell = random.Below(static_cast<uint32_t>(map.CellCount()));
    const auto terrain = map.terrain[cell];
    const auto fits = birds.gulls ? (terrain == Terrain::Shallows || map.IsCoast(cell)) : (terrain == Terrain::Grass || terrain == Terrain::Meadow || terrain == Terrain::Forest);
    goal = cell;
    if (fits && !map.grid.IsRim(cell))
      break;
  }
  if (goal == INVALID_CELL)
    return;
  const auto at = map.grid.GetCell(goal).centroid;
  const auto ground = std::max(map.GroundAt(at), .0f);
  birds.flock.SetGoal({at.x, ground + (birds.gulls ? 7.f : 9.f), at.y});
  birds.wander = random.Range(9.f, 18.f);
}
auto FlockSystem::Spawn() -> void {
  auto *mapSystem = FindMap(app);
  if (!mapSystem)
    return;
  const auto &map = mapSystem->GetMap();
  const auto Ground = [&map](const float x, const float z) {
    const auto h = map.GroundAt({x, z});
    return std::isfinite(h) ? std::max(h, .0f) : .0f;
  };
  for (auto f = 0; f < 4; ++f) {
    Birds birds;
    birds.gulls = f < 2;
    birds.scale = birds.gulls ? .8f : .55f;
    FlockSettings settings;
    settings.minHeight = birds.gulls ? 4.f : 5.f;
    settings.maxHeight = birds.gulls ? 11.f : 14.f;
    settings.minSpeed = birds.gulls ? 3.f : 5.f;
    settings.maxSpeed = birds.gulls ? 6.f : 9.f;
    settings.sightRadius = birds.gulls ? 4.5f : 3.f;
    settings.personalSpace = birds.gulls ? 1.3f : .8f;
    settings.flapRate = birds.gulls ? 2.4f : 5.f;
    const auto count = birds.gulls ? 12u : 18u;
    const auto cell = random.Below(static_cast<uint32_t>(map.CellCount()));
    const auto at = map.grid.GetCell(cell).centroid;
    birds.flock = Flock(count, {at.x, Ground(at.x, at.y) + 9.f, at.y}, settings, random.Next());
    birds.flock.SetGround(Ground);
    const auto body = birds.gulls ? "bird/gull" : "bird/swallow";
    const auto wing = birds.gulls ? "bird/gull" : "bird/swallow";
    for (size_t i = 0; i < count; ++i) {
      const auto Part = [&](const char *mesh, const char *material, const int side) {
        const auto id = FrontierWorld::Create<BirdObject>(app, {.mesh = MeshId(mesh), .material = MaterialId(material)});
        if (auto *tag = app.GetEntityComponent<Wing>(id))
          *tag = {f, static_cast<int>(i), side};
        return id;
      };
      const auto bodyId = Part("bird/body", body, 0);
      const auto leftId = Part("bird/wing_left", wing, -1);
      const auto rightId = Part("bird/wing_right", wing, 1);
      app.AddChildEntity(bodyId, leftId);
      app.AddChildEntity(bodyId, rightId);
      birds.bodies.push_back(bodyId);
      birds.left.push_back(leftId);
      birds.right.push_back(rightId);
      birds.heading.emplace_back(.0f, .0f, -1.f);
      birds.roll.push_back(.0f);
    }
    PickGoal(birds);
    flocks.push_back(std::move(birds));
  }
}
auto FlockSystem::Update(const float deltaTime) -> void {
  auto *mapSystem = FindMap(app);
  if (!mapSystem)
    return;
  if (mapSystem->GetGeneration() != generation) {
    generation = mapSystem->GetGeneration();
    Release();
    Spawn();
  }
  const auto dt = std::min(deltaTime, .05f);
  auto scene = app.GetScene();
  const auto *wind = scene ? scene->GetAnyComponent<Wind>() : nullptr;
  glm::vec3 air{};
  if (wind && glm::length(wind->direction) > 1e-4f) {
    const auto d = glm::normalize(wind->direction);
    air = glm::vec3(d.x, .0f, d.y) * wind->strength * 3.f;
  }
  for (auto &birds : flocks) {
    birds.wander -= dt;
    const auto centre = birds.flock.Centre();
    const auto goal = birds.flock.GetGoal();
    if (birds.wander <= .0f || glm::distance(glm::vec2(centre.x, centre.z), glm::vec2(goal.x, goal.z)) < 3.f)
      PickGoal(birds);
    birds.flock.Step(dt, air);
    const auto boids = birds.flock.GetBoids();
    for (size_t i = 0; i < boids.size(); ++i) {
      const auto &boid = boids[i];
      const auto speed = glm::length(boid.velocity);
      if (speed < 1e-4f)
        continue;
      const auto forward = boid.velocity / speed;
      // Banked into the turn, by how fast the heading is swinging round.
      const auto turn = glm::cross(birds.heading[i], forward).y / std::max(dt, 1e-4f);
      birds.heading[i] = forward;
      const auto wanted = std::clamp(turn * .35f, -.9f, .9f);
      birds.roll[i] += (wanted - birds.roll[i]) * std::min(1.f, dt * 6.f);
      // Level enough for the look-at: a bird diving straight down still has an up.
      const auto look = std::abs(forward.y) > .98f ? glm::normalize(forward + glm::vec3(.0f, .0f, .05f)) : forward;
      if (auto *transform = app.GetEntityComponent<Transform>(birds.bodies[i])) {
        transform->position = boid.position;
        transform->rotation = glm::quatLookAt(look, UP) * glm::angleAxis(birds.roll[i], glm::vec3(.0f, .0f, -1.f));
        transform->scale = glm::vec3(birds.scale);
        app.MarkTransformDirty(birds.bodies[i]);
      }
      // The beat: a wing goes up through more of the stroke than it comes down, as a bird's does,
      // and a gliding bird holds its wings a little above level.
      const auto beat = std::sin(boid.phase * 6.2831853f);
      const auto flap = (beat > .0f ? beat * .95f : beat * .6f) * (.25f + boid.effort) + .12f;
      if (auto *transform = app.GetEntityComponent<Transform>(birds.left[i])) {
        transform->rotation = glm::angleAxis(-flap, glm::vec3(.0f, .0f, 1.f));
        app.MarkTransformDirty(birds.left[i]);
      }
      if (auto *transform = app.GetEntityComponent<Transform>(birds.right[i])) {
        transform->rotation = glm::angleAxis(flap, glm::vec3(.0f, .0f, 1.f));
        app.MarkTransformDirty(birds.right[i]);
      }
    }
  }
}
