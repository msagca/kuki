#include <GLFW/glfw3.h>
#include <algorithm>
#include <application.hpp>
#include <camera_system.hpp>
#include <cmath>
#include <command_line.hpp>
#include <controls.hpp>
#include <format>
#include <frontier.hpp>
#include <frontier_world.hpp>
#include <glm/gtc/quaternion.hpp>
#include <map_system.hpp>
#include <material_handle.hpp>
#include <mesh_asset.hpp>
#include <particle_emitter.hpp>
#include <placement_system.hpp>
#include <props.hpp>
#include <random.hpp>
#include <scene.hpp>
#include <spdlog/spdlog.h>
#include <transform.hpp>
#include <wind.hpp>
using namespace kuki;
using namespace frontier;
namespace {
constexpr glm::vec3 UP{.0f, 1.f, .0f};
constexpr glm::vec3 GOOD{.3f, 1.f, .45f};
constexpr glm::vec3 BAD{1.f, .25f, .2f};
/// @brief The turn a building is given for an angle on the map's plane. The plane's second axis is
/// the world's Z, so a turn that is counter-clockwise on the plane is clockwise about the world's Y.
auto Turn(const float angle) -> glm::quat {
  return glm::angleAxis(-angle, UP);
}
auto Display(const glm::vec3 &linear, const float alpha = 1.f) -> glm::vec4 {
  return {glm::pow(linear, glm::vec3(1.f / 2.2f)), alpha};
}
auto Systems(Application &app) -> std::pair<MapSystem *, CameraSystem *> {
  auto *frontier = dynamic_cast<Frontier *>(&app);
  if (!frontier)
    return {nullptr, nullptr};
  return {frontier->GetSystem<MapSystem>(), frontier->GetSystem<CameraSystem>()};
}
} // namespace
PlacementSystem::PlacementSystem(Application &app)
  : System(std::in_place_type<PlacementSystem>, app) {}
auto PlacementSystem::GetPlaced() const -> const std::vector<std::optional<Placed>> & {
  return placed;
}
auto PlacementSystem::ClearGhost() -> void {
  for (const auto id : ghost)
    app.DeleteEntity(id);
  ghost.clear();
}
auto PlacementSystem::Select(const int type) -> void {
  ClearGhost();
  selected = type;
  footprint.clear();
  if (type < 0)
    return;
  AddMaterial(app, "ghost/good", {.albedo = GOOD, .toon = .0f, .alpha = .5f, .unlit = true});
  AddMaterial(app, "ghost/bad", {.albedo = BAD, .toon = .0f, .alpha = .5f, .unlit = true});
  for (const auto &part : Buildings(app)[static_cast<size_t>(type)].parts) {
    ghost.push_back(FrontierWorld::Create<MeshObject>(app, {.mesh = MeshId(part.mesh), .material = MaterialId("ghost/good")}));
  }
}
auto PlacementSystem::Stand(const std::vector<CellIndex> &cells) const -> glm::vec3 {
  auto [map, camera] = Systems(app);
  if (!map || cells.empty())
    return {};
  const auto &ground = map->GetMap();
  auto top = -1e9f;
  for (const auto cell : cells)
    top = std::max(top, ground.height[cell]);
  return ToWorld(ground.grid.Middle(cells), top);
}
auto PlacementSystem::Build(const int type, const std::vector<CellIndex> &cells, const float turn) -> void {
  auto [mapSystem, camera] = Systems(app);
  if (!mapSystem)
    return;
  auto &map = mapSystem->GetMap();
  const auto id = static_cast<int>(placed.size());
  const auto &kind = Buildings(app)[static_cast<size_t>(type)];
  Placed building{.type = type, .cells = cells, .position = Stand(cells), .angle = turn};
  const auto rotation = Turn(turn);
  for (const auto &part : kind.parts) {
    const auto entity = FrontierWorld::Create<StructureObject>(app, {.mesh = MeshId(part.mesh), .material = MaterialId(part.material)});
    if (auto *structure = app.GetEntityComponent<Structure>(entity))
      structure->id = id;
    if (auto *transform = app.GetEntityComponent<Transform>(entity)) {
      transform->position = building.position + (part.spins ? rotation * part.pivot : glm::vec3(.0f));
      transform->rotation = rotation;
    }
    app.MarkTransformDirty(entity);
    building.parts.push_back(entity);
    if (part.spins)
      building.sails = entity;
  }
  if (kind.smokes) {
    // Smoke from the chimney, carried off by the wind like every other drifting thing.
    building.smoke = FrontierWorld::Create<ParticleObject>(app);
    if (auto *smoke = app.GetEntityComponent<ParticleEmitter>(building.smoke)) {
      smoke->capacity = 192;
      smoke->rate = 9.f;
      smoke->blend = ParticleBlend::Alpha;
      smoke->opacity = .35f;
      smoke->radius = .04f;
      smoke->lifetime = 5.f;
      smoke->speed = .5f;
      smoke->spread = 10.f;
      smoke->gravity = {.0f, .25f, .0f};
      smoke->drag = .5f;
      smoke->turbulence = .5f;
      smoke->startSize = .1f;
      smoke->endSize = .9f;
      smoke->startColor = {.85f, .85f, .85f};
      smoke->endColor = {.6f, .62f, .66f};
      smoke->intensity = 1.f;
    }
    if (auto *transform = app.GetEntityComponent<Transform>(building.smoke))
      transform->position = building.position + rotation * kind.chimney;
    app.MarkTransformDirty(building.smoke);
  }
  for (const auto cell : cells) {
    map.occupant[cell] = id;
    map.RefreshNavigation(cell);
  }
  mapSystem->RefreshLayers();
  spdlog::info("[Frontier] Built a {} over {} cell{}", kind.name, cells.size(), cells.size() == 1 ? "" : "s");
  placed.push_back(std::move(building));
}
auto PlacementSystem::Demolish(const int id) -> void {
  auto [mapSystem, camera] = Systems(app);
  if (id < 0 || static_cast<size_t>(id) >= placed.size() || !placed[static_cast<size_t>(id)])
    return;
  auto &building = *placed[static_cast<size_t>(id)];
  for (const auto entity : building.parts)
    app.DeleteEntity(entity);
  if (building.smoke)
    app.DeleteEntity(building.smoke);
  if (mapSystem) {
    auto &map = mapSystem->GetMap();
    for (const auto cell : building.cells) {
      map.occupant[cell] = -1;
      map.RefreshNavigation(cell);
    }
    mapSystem->RefreshLayers();
  }
  placed[static_cast<size_t>(id)].reset();
}
auto PlacementSystem::Spin(const float dt) -> void {
  auto scene = app.GetScene();
  const auto *wind = scene ? scene->GetAnyComponent<Wind>() : nullptr;
  const auto strength = wind ? wind->strength : 1.f;
  for (auto &entry : placed) {
    if (!entry || !entry->sails)
      continue;
    entry->spin = std::fmod(entry->spin + dt * (.4f + strength * 1.6f), 6.2831853f);
    if (auto *transform = app.GetEntityComponent<Transform>(entry->sails)) {
      transform->rotation = Turn(entry->angle) * glm::angleAxis(entry->spin, glm::vec3(.0f, .0f, 1.f));
      app.MarkTransformDirty(entry->sails);
    }
  }
}
auto PlacementSystem::Update(const float deltaTime) -> void {
  auto [mapSystem, camera] = Systems(app);
  if (!mapSystem || !camera)
    return;
  // A new map takes every building with the old one.
  if (mapSystem->GetGeneration() != generation) {
    generation = mapSystem->GetGeneration();
    for (size_t i = 0; i < placed.size(); ++i)
      if (placed[i]) {
        for (const auto entity : placed[i]->parts)
          app.DeleteEntity(entity);
        if (placed[i]->smoke)
          app.DeleteEntity(placed[i]->smoke);
      }
    placed.clear();
    if (HasFlag(GetCommandLineArguments(), MapSystem::DEMO_FLAG))
      Demo();
  }
  auto &map = mapSystem->GetMap();
  const auto &types = Buildings(app);
  for (size_t i = 0; i < types.size() && i < BUILD_KEYS; ++i)
    if (app.IsBindingPressed(BuildBinding(i)))
      Select(selected == static_cast<int>(i) ? -1 : static_cast<int>(i));
  // A right click puts the building down again, as its key does. Escape is the settings menu's now.
  if (selected >= 0 && app.IsInputPressed(GLFW_MOUSE_BUTTON_RIGHT))
    Select(-1);
  camera->SetTurnKeys(selected < 0);
  mapSystem->SetPathEnabled(selected < 0);
  const auto &cursor = mapSystem->GetCursor();
  const auto hovered = mapSystem->GetHoveredCell();
  if (selected >= 0) {
    const auto rate = app.IsBindingHeld(MODIFIER) ? .6f : 2.2f;
    if (app.IsBindingHeld(TURN_LEFT))
      angle += rate * deltaTime;
    if (app.IsBindingHeld(TURN_RIGHT))
      angle -= rate * deltaTime;
    const auto &type = types[static_cast<size_t>(selected)];
    footprint = cursor ? map.grid.Footprint({cursor->x, cursor->z}, angle, type.shape) : std::vector<CellIndex>{};
    valid = footprint.size() == type.shape.size() && std::ranges::all_of(footprint, [&](const CellIndex c) { return map.IsFree(c); });
    const auto at = footprint.empty() ? (cursor ? *cursor : glm::vec3(.0f, -100.f, .0f)) : Stand(footprint);
    const auto rotation = Turn(angle);
    for (size_t i = 0; i < ghost.size() && i < type.parts.size(); ++i) {
      if (auto *transform = app.GetEntityComponent<Transform>(ghost[i])) {
        transform->position = at + (type.parts[i].spins ? rotation * type.parts[i].pivot : glm::vec3(.0f));
        transform->rotation = rotation;
        app.MarkTransformDirty(ghost[i]);
      }
      // The colour is the pool's, so a preview turning red moves to the red pool -- and only when it
      // turns, not every frame it stays red.
      if (auto pool = app.GetEntityPool(ghost[i]); pool && pool.material != MaterialId(valid ? "ghost/good" : "ghost/bad")) {
        pool.material = MaterialId(valid ? "ghost/good" : "ghost/bad");
        app.MoveEntityToPool(ghost[i], pool);
      }
    }
    mapSystem->Highlight(footprint, valid ? GOOD : BAD);
    if (valid && app.IsInputPressed(GLFW_MOUSE_BUTTON_LEFT))
      Build(selected, footprint, angle);
  } else {
    // Over a building, its cells; over anything else, the one cell.
    const auto occupant = hovered != INVALID_CELL ? map.occupant[hovered] : -1;
    if (occupant >= 0 && placed[static_cast<size_t>(occupant)]) {
      mapSystem->Highlight(placed[static_cast<size_t>(occupant)]->cells, {.4f, .7f, 1.f});
      if (app.IsBindingPressed(DEMOLISH))
        Demolish(occupant);
    } else if (hovered != INVALID_CELL) {
      const std::vector one{hovered};
      mapSystem->Highlight(one, {1.f, .95f, .8f});
    } else
      mapSystem->Highlight({}, glm::vec3(1.f));
  }
  Spin(deltaTime);
  DrawPanel();
}
auto PlacementSystem::Demo() -> void {
  auto [mapSystem, camera] = Systems(app);
  if (!mapSystem)
    return;
  auto &map = mapSystem->GetMap();
  const auto &types = Buildings(app);
  Random random(map.seed, 91u);
  // Round where the camera starts, so a picture taken at once shows them.
  const auto middle = mapSystem->GetStart();
  for (size_t type = 0; type < types.size(); ++type)
    for (auto copy = 0, attempts = 0; copy < 2 && attempts < 400; ++attempts) {
      const auto at = middle + glm::vec2(random.Range(-9.f, 9.f), random.Range(-7.f, 7.f));
      const auto turn = random.Range(.0f, 6.2831853f);
      const auto cells = map.grid.Footprint(at, turn, types[type].shape);
      if (cells.size() != types[type].shape.size() || !std::ranges::all_of(cells, [&](const CellIndex c) { return map.IsFree(c); }))
        continue;
      Build(static_cast<int>(type), cells, turn);
      ++copy;
    }
}
auto PlacementSystem::DrawPanel() -> void {
  // Only while a building is in hand, and then only which one, and turned how far. The catalogue and
  // the keys that pick from it are in the settings' controls; the line that said how to turn, build
  // and cancel is gone with the rest of the hints.
  if (selected < 0)
    return;
  const auto *context = app.GetGraphicsContext();
  if (!context)
    return;
  const auto [width, height] = context->GetSurfaceSize();
  if (width <= 0 || height <= 0)
    return;
  const auto unit = static_cast<float>(height) / 1080.f;
  const auto &types = Buildings(app);
  if (static_cast<size_t>(selected) >= types.size())
    return;
  const auto &type = types[static_cast<size_t>(selected)];
  const auto degrees = std::fmod(std::fmod(angle * 57.2958f, 360.f) + 360.f, 360.f);
  app.GetOverlay().DrawText(std::format("{}   {:.0f} degrees", type.name, degrees), {.0f, -48.f * unit}, 22.f * unit, Display(valid ? GOOD : BAD), TextAnchor::Bottom);
}
