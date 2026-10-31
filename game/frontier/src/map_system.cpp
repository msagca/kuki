#include <GLFW/glfw3.h>
#include <algorithm>
#include <application.hpp>
#include <bounding_box.hpp>
#include <camera_system.hpp>
#include <cctype>
#include <chrono>
#include <cmath>
#include <command_line.hpp>
#include <format>
#include <frontier.hpp>
#include <frontier_world.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/quaternion.hpp>
#include <map_system.hpp>
#include <material_asset.hpp>
#include <material_handle.hpp>
#include <mesh_asset.hpp>
#include <mesh_builder.hpp>
#include <mesh_handle.hpp>
#include <particle_emitter.hpp>
#include <props.hpp>
#include <spdlog/spdlog.h>
#include <transform.hpp>
using namespace kuki;
using namespace frontier;
namespace {
constexpr glm::vec3 UP{.0f, 1.f, .0f};
/// How many classes a layer can colour cells in, each a mesh and a material of its own.
constexpr int LAYER_CLASSES = 6;
/// Below the deep sea's floor, for the faces at the map's rim to come down to.
constexpr float FLOOR = -3.f;
auto TerrainName(const Terrain terrain) -> std::string {
  constexpr std::array names{"deep", "shallows", "sand", "grass", "meadow", "forest", "hills", "rock", "snow"};
  return names[static_cast<size_t>(terrain)];
}
/// @brief What the face of a step down from a cell is made of.
auto CliffOf(const Terrain terrain) -> std::string {
  switch (terrain) {
  case Terrain::Deep:
  case Terrain::Shallows:
    return "cliff/seabed";
  case Terrain::Sand:
    return "cliff/sand";
  case Terrain::Grass:
  case Terrain::Meadow:
  case Terrain::Forest:
    return "cliff/earth";
  default:
    return "cliff/stone";
  }
}
auto Display(const glm::vec3 &linear, const float alpha = 1.f) -> glm::vec4 {
  return {glm::pow(linear, glm::vec3(1.f / 2.2f)), alpha};
}
} // namespace
MapSystem::MapSystem(Application &app)
  : System(std::in_place_type<MapSystem>, app) {}
auto MapSystem::GetMap() -> TerrainMap & {
  return map;
}
auto MapSystem::GetCursor() const -> const std::optional<glm::vec3> & {
  return cursor;
}
auto MapSystem::GetHoveredCell() const -> CellIndex {
  return hovered;
}
auto MapSystem::GetStart() const -> glm::vec2 {
  return start;
}
auto MapSystem::GetGeneration() const -> uint32_t {
  return generation;
}
auto MapSystem::SetPathEnabled(const bool enabled) -> void {
  pathEnabled = enabled;
}
auto MapSystem::RefreshLayers() -> void {
  layersDirty = true;
}
auto MapSystem::PostStart() -> void {
  const auto arguments = GetCommandLineArguments();
  if (const auto value = GetFlagValue(arguments, SEED_FLAG)) {
    try {
      seed = std::stoull(*value);
    } catch (...) {
      spdlog::warn("[Frontier] --seed wants a number, not {}", *value);
    }
  } else
    seed = static_cast<uint64_t>(std::chrono::system_clock::now().time_since_epoch().count()) % 100000u;
  if (const auto value = GetFlagValue(arguments, LAYER_FLAG)) {
    for (size_t i = 0; i < LAYER_NAMES.size(); ++i) {
      std::string name = LAYER_NAMES[i];
      std::ranges::transform(name, name.begin(), [](const unsigned char c) { return static_cast<char>(std::tolower(c)); });
      if (name == *value)
        layer = static_cast<Layer>(i);
    }
  }
  // The layer meshes and the highlight, made once and rebuilt in place from then on.
  for (auto k = 0; k < LAYER_CLASSES; ++k) {
    AddMaterial(app, std::format("layer/{}", k), {.albedo = glm::vec3(1.f), .toon = .0f, .alpha = .45f, .unlit = true});
    AddMesh(app, std::format("layer/{}", k), {});
  }
  AddMaterial(app, "highlight", {.albedo = glm::vec3(1.f), .toon = .0f, .alpha = .9f, .unlit = true});
  AddMesh(app, "highlight", {});
  AddMaterial(app, "path", {.albedo = {1.f, .85f, .3f}, .toon = .0f, .alpha = .9f, .unlit = true});
  AddMesh(app, "path", {});
  Generate();
  if (HasFlag(arguments, DEMO_FLAG))
    DemoPath();
}
auto MapSystem::DemoPath() -> void {
  Random random(map.seed, 77u);
  std::vector<CellIndex> walkable;
  for (CellIndex c = 0; c < map.CellCount(); ++c)
    if (!map.nav.IsBlocked(c) && !map.grid.IsRim(c))
      walkable.push_back(c);
  if (walkable.size() < 2)
    return;
  auto best = .0f;
  for (auto attempt = 0; attempt < 60; ++attempt) {
    const auto a = map.grid.GetCell(walkable[random.Below(static_cast<uint32_t>(walkable.size()))]).centroid;
    const auto b = map.grid.GetCell(walkable[random.Below(static_cast<uint32_t>(walkable.size()))]).centroid;
    // Far apart, but not so far the camera cannot see both.
    const auto distance = glm::distance(a, b);
    if (distance < best || distance > 30.f)
      continue;
    auto found = map.nav.FindPath(a, b, .12f);
    if (found.empty())
      continue;
    best = distance;
    pathStart = a;
    path = std::move(found);
  }
  pathLength = .0f;
  for (size_t i = 1; i < path.size(); ++i)
    pathLength += glm::distance(path[i - 1], path[i]);
  BuildPath();
}
auto MapSystem::Clear() -> void {
  for (const auto id : entities)
    app.DeleteEntity(id);
  entities.clear();
}
auto MapSystem::Generate() -> void {
  Clear();
  map = GenerateMap({.seed = seed});
  const auto [low, high] = map.grid.NeighbourRange();
  spdlog::info("[Frontier] Map {}: {} cells with {} to {} neighbours, settled with {} repairs in {} attempts, {:.0f} ms", seed, map.CellCount(), low, high, map.repairs, map.attempts, map.elapsed * 1000.f);
  {
    std::array<int, TERRAIN_COUNT> counts{};
    for (const auto terrain : map.terrain)
      ++counts[static_cast<size_t>(terrain)];
    std::string shares;
    for (size_t t = 0; t < TERRAIN_COUNT; ++t)
      shares += std::format("{} {:.0f}%  ", TERRAIN[t].name, 100.f * static_cast<float>(counts[t]) / static_cast<float>(map.CellCount()));
    spdlog::info("[Frontier] Terrain: {}", shares);
  }
  BuildGround();
  BuildWater();
  BuildProps();
  // The layers, the highlight and the path each have one entity, standing at the origin, whose mesh
  // is rebuilt as they change.
  for (auto k = 0; k < LAYER_CLASSES; ++k)
    Place(std::format("layer/{}", k), std::format("layer/{}", k), {});
  Place("highlight", "highlight", {});
  Place("path", "path", {});
  path.clear();
  pathStart.reset();
  BuildPath();
  highlighted.clear();
  Highlight({}, glm::vec3(1.f));
  layersDirty = true;
  // The cell nearest the middle with the most free land round it.
  const auto middle = (map.grid.GetMin() + map.grid.GetMax()) * .5f;
  start = middle;
  auto best = -1e9f;
  for (CellIndex c = 0; c < map.CellCount(); ++c) {
    if (!map.IsFree(c))
      continue;
    const auto around = map.grid.Within(c, 2);
    const auto free = static_cast<float>(std::ranges::count_if(around, [&](const CellIndex n) { return map.IsFree(n); }));
    const auto score = free - glm::distance(map.grid.GetCell(c).centroid, middle) * .5f;
    if (score > best) {
      best = score;
      start = map.grid.GetCell(c).centroid;
    }
  }
  if (auto *frontier = dynamic_cast<Frontier *>(&app)) {
    auto *camera = frontier->GetSystem<CameraSystem>();
    camera->SetBounds(map.grid.GetMin() + 4.f, map.grid.GetMax() - 4.f);
    camera->LookAt(start);
  }
  ++generation;
}
auto MapSystem::Place(const std::string &mesh, const std::string &material, const glm::vec3 &position, const float yaw, const float scale) -> EntityID {
  const auto id = FrontierWorld::Create<MeshObject>(app);
  if (!id)
    return id;
  if (auto *handle = app.GetEntityComponent<MeshHandle>(id))
    handle->assetId = MeshId(mesh);
  if (auto *handle = app.GetEntityComponent<MaterialHandle>(id))
    handle->assetId = MaterialId(material);
  if (auto *transform = app.GetEntityComponent<Transform>(id)) {
    transform->position = position;
    transform->rotation = glm::angleAxis(yaw, UP);
    transform->scale = glm::vec3(scale);
  }
  // Bounds to cull by for the props, and none for the map's own meshes. Those are rebuilt with every
  // map and would need them made again, there are few enough of them to draw whatever the camera
  // sees -- and the sun's shadow is fitted round every box in the scene, so a sea reaching to the
  // horizon would stretch one shadow map over all of it.
  const auto own = mesh.starts_with("map/") || mesh.starts_with("layer") || mesh == "highlight" || mesh == "path";
  if (const auto *asset = app.GetAsset<MeshAsset>(MeshId(mesh)); asset && !own)
    if (auto *bounds = app.GetEntityComponent<BoundingBox>(id))
      *bounds = asset->bounds;
  app.MarkTransformDirty(id);
  entities.push_back(id);
  return id;
}
auto MapSystem::Scatter(const CellIndex cell, Random &random, const float reach) const -> glm::vec2 {
  const auto &shape = map.grid.GetCell(cell);
  const auto i = random.Below(static_cast<uint32_t>(shape.corners.size()));
  const auto a = map.grid.GetCorner(shape.corners[i]) - shape.centroid;
  const auto b = map.grid.GetCorner(shape.corners[(i + 1) % shape.corners.size()]) - shape.centroid;
  const auto s = random.Float();
  // The square root spreads the points evenly over the triangle rather than bunching them in the middle.
  const auto t = std::sqrt(random.Float()) * reach;
  return shape.centroid + (a * (1.f - s) + b * s) * t;
}
auto MapSystem::BuildGround() -> void {
  const auto &grid = map.grid;
  std::array<MeshBuilder, TERRAIN_COUNT> tops;
  std::vector<std::pair<std::string, MeshBuilder>> cliffs;
  const auto Cliff = [&](const std::string &name) -> MeshBuilder & {
    for (auto &[key, builder] : cliffs)
      if (key == name)
        return builder;
    return cliffs.emplace_back(name, MeshBuilder{}).second;
  };
  // The tops: a fan from each cell's middle to its corners, flat for a terrace and to a point for a peak.
  for (CellIndex c = 0; c < grid.CellCount(); ++c) {
    const auto &shape = grid.GetCell(c);
    const auto terrain = map.terrain[c];
    std::vector<glm::vec3> ring;
    for (size_t i = 0; i < shape.corners.size(); ++i)
      ring.push_back(ToWorld(grid.GetCorner(shape.corners[i]), map.SideHeights(c, i).first));
    tops[static_cast<size_t>(terrain)].Fan(ToWorld(shape.centroid, map.height[c] + map.peak[c]), ring, UP);
  }
  // The steps: across every side, the higher of the two cells faces down to the lower, and a cell
  // at the rim faces down to the floor.
  for (const auto &edge : grid.GetEdges()) {
    const auto &left = grid.GetCell(edge.left);
    const auto side = static_cast<size_t>(std::ranges::find(left.corners, edge.from) - left.corners.begin());
    const auto [a0, a1] = map.SideHeights(edge.left, side);
    auto b0 = FLOOR;
    auto b1 = FLOOR;
    if (edge.right != INVALID_CELL) {
      const auto &right = grid.GetCell(edge.right);
      const auto other = static_cast<size_t>(std::ranges::find(right.corners, edge.to) - right.corners.begin());
      const auto heights = map.SideHeights(edge.right, other);
      // The other cell runs the side the other way: its start is this side's end.
      b0 = heights.second;
      b1 = heights.first;
    }
    if (std::abs((a0 + a1) - (b0 + b1)) < .02f)
      continue;
    const auto leftHigher = a0 + a1 > b0 + b1;
    const auto high = leftHigher ? edge.left : edge.right;
    const auto p = grid.GetCorner(edge.from);
    const auto q = grid.GetCorner(edge.to);
    const auto out = (p + q) * .5f - grid.GetCell(high).centroid;
    const auto [h0, h1] = leftHigher ? std::pair{a0, a1} : std::pair{b0, b1};
    const auto [l0, l1] = leftHigher ? std::pair{b0, b1} : std::pair{a0, a1};
    Cliff(CliffOf(map.terrain[high])).OrientedQuad(ToWorld(p, h0), ToWorld(q, h1), ToWorld(q, l1), ToWorld(p, l0), {out.x, .0f, out.y});
  }
  for (size_t t = 0; t < TERRAIN_COUNT; ++t) {
    const auto name = "map/top/" + TerrainName(static_cast<Terrain>(t));
    AddMesh(app, name, tops[t].Take());
    Place(name, "ground/" + TerrainName(static_cast<Terrain>(t)), {});
  }
  for (const auto material : {"cliff/seabed", "cliff/sand", "cliff/earth", "cliff/stone"}) {
    const auto name = std::string("map/") + material;
    AddMesh(app, name, Cliff(material).Take());
    Place(name, material, {});
  }
}
auto MapSystem::BuildWater() -> void {
  const auto &grid = map.grid;
  MeshBuilder deep;
  MeshBuilder far;
  MeshBuilder shallows;
  MeshBuilder coast;
  MeshBuilder foam;
  // Each water cell's fan, each triangle cut in four so the waves have corners to move.
  const auto Fine = [](MeshBuilder &mesh, const glm::vec3 &a, const glm::vec3 &b, const glm::vec3 &c) {
    const auto ab = (a + b) * .5f;
    const auto bc = (b + c) * .5f;
    const auto ca = (c + a) * .5f;
    mesh.Oriented(a, ab, ca, UP).Oriented(ab, b, bc, UP).Oriented(ca, bc, c, UP).Oriented(ab, bc, ca, UP);
  };
  for (CellIndex c = 0; c < grid.CellCount(); ++c) {
    if (!IsWater(map.terrain[c]))
      continue;
    const auto touchesLand = std::ranges::any_of(grid.GetCell(c).neighbours, [&](const CellIndex n) { return !IsWater(map.terrain[n]); });
    auto &mesh = map.terrain[c] == Terrain::Deep ? deep : (touchesLand ? coast : shallows);
    const auto &shape = grid.GetCell(c);
    for (size_t i = 0; i < shape.corners.size(); ++i)
      Fine(mesh, ToWorld(shape.centroid, .0f), ToWorld(grid.GetCorner(shape.corners[i]), .0f), ToWorld(grid.GetCorner(shape.corners[(i + 1) % shape.corners.size()]), .0f));
  }
  // The open sea past the map's ragged rim, to the horizon. Lower than the cells' water by a hair, so
  // where the two overlap at the rim the cells' is the one seen.
  const auto low = grid.GetMin();
  const auto high = grid.GetMax();
  const auto inner0 = low + 2.f;
  const auto inner1 = high - 2.f;
  for (auto pass = 0; pass < 2; ++pass) {
    const auto step = pass == 0 ? 2.f : 16.f;
    const auto reach = pass == 0 ? 40.f : 400.f;
    const auto from = low - reach;
    const auto to = high + reach;
    for (auto x = from.x; x < to.x; x += step)
      for (auto z = from.y; z < to.y; z += step) {
        const glm::vec2 a{x, z};
        const glm::vec2 b{x + step, z + step};
        const auto skip = pass == 0 ? (a.x >= inner0.x && b.x <= inner1.x && a.y >= inner0.y && b.y <= inner1.y) : (a.x >= low.x - 40.f && b.x <= high.x + 40.f && a.y >= low.y - 40.f && b.y <= high.y + 40.f);
        if (skip)
          continue;
        const auto y = pass == 0 ? -.03f : -.06f;
        (pass == 0 ? deep : far).OrientedQuad({a.x, y, a.y}, {b.x, y, a.y}, {b.x, y, b.y}, {a.x, y, b.y}, UP);
      }
  }
  // Foam along the shore: a band on the water's side of every side between land and sea, in a few
  // pieces so it heaves with the waves.
  for (const auto &edge : grid.GetEdges()) {
    if (edge.right == INVALID_CELL)
      continue;
    const auto leftWet = IsWater(map.terrain[edge.left]);
    if (leftWet == IsWater(map.terrain[edge.right]))
      continue;
    const auto water = leftWet ? edge.left : edge.right;
    const auto p = grid.GetCorner(edge.from);
    const auto q = grid.GetCorner(edge.to);
    const auto middle = grid.GetCell(water).centroid;
    constexpr int PIECES = 3;
    for (auto k = 0; k < PIECES; ++k) {
      const auto s0 = static_cast<float>(k) / PIECES;
      const auto s1 = static_cast<float>(k + 1) / PIECES;
      const auto e0 = p + (q - p) * s0;
      const auto e1 = p + (q - p) * s1;
      const auto i0 = e0 + (middle - e0) * .24f;
      const auto i1 = e1 + (middle - e1) * .24f;
      foam.OrientedQuad(ToWorld(e0, .05f), ToWorld(e1, .05f), ToWorld(i1, .05f), ToWorld(i0, .05f), UP);
    }
  }
  const std::pair<const char *, MeshBuilder *> parts[]{{"water/deep", &deep}, {"water/far", &far}, {"water/shallows", &shallows}, {"water/coast", &coast}, {"water/foam", &foam}};
  for (const auto &[name, mesh] : parts) {
    AddMesh(app, std::string("map/") + name, mesh->Take());
    Place(std::string("map/") + name, name, {});
  }
}
auto MapSystem::BuildProps() -> void {
  const auto &grid = map.grid;
  Random random(map.seed, 0x9b05688c2b3e6c1full);
  const auto Variant = [&] { return static_cast<int>(random.Below(VARIANTS)); };
  const auto Yaw = [&] { return random.Range(.0f, 6.2831853f); };
  const auto Tree = [&](const glm::vec3 &at, const bool pine, const float scale) {
    const auto v = Variant();
    const auto yaw = Yaw();
    const auto kind = pine ? "pine" : "broad";
    Place(Prop(kind, v, "trunk"), "prop/bark", at, yaw, scale);
    Place(Prop(kind, v, "crown"), pine ? "prop/pine" : (random.Chance(.12f) ? "prop/leaves_autumn" : "prop/leaves"), at, yaw, scale);
  };
  for (CellIndex c = 0; c < grid.CellCount(); ++c) {
    const auto terrain = map.terrain[c];
    const auto top = map.height[c];
    const auto At = [&](const glm::vec2 &p) { return ToWorld(p, top); };
    switch (terrain) {
    case Terrain::Forest: {
      // Pines where the ground is high or the cell is far from the meadows, broadleaves elsewhere.
      const auto pines = std::ranges::any_of(grid.GetCell(c).neighbours, [&](const CellIndex n) { return map.terrain[n] == Terrain::Hills; });
      const auto count = 3 + static_cast<int>(random.Below(3));
      for (auto i = 0; i < count; ++i)
        Tree(At(Scatter(c, random, .7f)), pines ? random.Chance(.85f) : random.Chance(.3f), random.Range(.8f, 1.25f));
      if (random.Chance(.5f))
        Place(Prop("bush", Variant()), "prop/bush", At(Scatter(c, random)), Yaw(), random.Range(.7f, 1.1f));
      break;
    }
    case Terrain::Grass:
    case Terrain::Meadow: {
      const auto tufts = terrain == Terrain::Meadow ? 2 : static_cast<int>(random.Below(4));
      for (auto i = 0; i < tufts; ++i)
        Place(Prop("tuft", Variant()), "prop/grass", At(Scatter(c, random, .85f)), Yaw(), random.Range(.8f, 1.3f));
      if (terrain == Terrain::Meadow) {
        const auto flowers = 2 + static_cast<int>(random.Below(3));
        constexpr std::array colours{"prop/flower_white", "prop/flower_yellow", "prop/flower_pink"};
        const auto colour = colours[random.Below(3)];
        for (auto i = 0; i < flowers; ++i) {
          const auto v = Variant();
          const auto at = At(Scatter(c, random, .8f));
          const auto yaw = Yaw();
          Place(Prop("flowers", v, "stems"), "prop/stems", at, yaw);
          Place(Prop("flowers", v, "heads"), colour, at, yaw);
        }
      }
      if (map.resource[c] < 0 && random.Chance(terrain == Terrain::Grass ? .07f : .03f))
        Tree(At(Scatter(c, random, .5f)), random.Chance(.3f), random.Range(.9f, 1.3f));
      break;
    }
    case Terrain::Hills:
      if (map.resource[c] < 0) {
        if (random.Chance(.2f))
          Place(Prop("pebbles", Variant()), "prop/stone", At(Scatter(c, random)), Yaw(), random.Range(.7f, 1.2f));
        if (random.Chance(.25f))
          Tree(At(Scatter(c, random, .5f)), true, random.Range(.8f, 1.1f));
        if (random.Chance(.5f))
          Place(Prop("tuft", Variant()), "prop/grass", At(Scatter(c, random)), Yaw(), .9f);
      }
      break;
    case Terrain::Sand:
      if (random.Chance(.15f))
        Place(Prop("pebbles", Variant()), "prop/stone", At(Scatter(c, random)), Yaw(), random.Range(.5f, .8f));
      break;
    case Terrain::Rock:
      if (random.Chance(.25f)) {
        const auto p = Scatter(c, random, .5f);
        Place(Prop("boulder", Variant()), "prop/stone", ToWorld(p, map.GroundAt(p) - .05f), Yaw(), random.Range(.6f, 1.f));
      }
      break;
    default:
      break;
    }
  }
  // The resources, each drawn over every cell of its site.
  for (const auto &site : map.sites) {
    for (const auto cell : site.cells) {
      const auto top = map.height[cell];
      const auto At = [&](const glm::vec2 &p) { return ToWorld(p, top); };
      switch (site.kind) {
      case Resource::Stone: {
        // One great boulder at the middle of each cell, lying along the site, and stones round it.
        Place(Prop("boulder", Variant()), "prop/stone", At(grid.GetCell(cell).centroid), -site.angle, random.Range(1.3f, 1.8f));
        for (auto i = 0; i < 2; ++i)
          Place(Prop("boulder", Variant()), "prop/stone", At(Scatter(cell, random, .65f)), Yaw(), random.Range(.6f, .9f));
        Place(Prop("pebbles", Variant()), "prop/stone", At(Scatter(cell, random)), Yaw());
        break;
      }
      case Resource::Ore: {
        const auto v = Variant();
        const auto at = At(grid.GetCell(cell).centroid);
        const auto yaw = Yaw();
        Place(Prop("crystals", v, "base"), "prop/crystal_rock", at, yaw, 1.4f);
        Place(Prop("crystals", v), "prop/crystal", at, yaw, 1.4f);
        Place(Prop("crystals", Variant()), "prop/crystal", At(Scatter(cell, random, .6f)), Yaw(), .8f);
        break;
      }
      case Resource::Berries:
        for (auto i = 0; i < 2; ++i) {
          const auto v = Variant();
          const auto at = At(Scatter(cell, random, .55f));
          const auto yaw = Yaw();
          Place(Prop("bush", v), "prop/bush", at, yaw);
          Place(Prop("berries", v), "prop/berries", at, yaw);
        }
        break;
      case Resource::Fish: {
        // Rings spreading where the fish rise, and now and then a splash.
        const auto at = ToWorld(grid.GetCell(cell).centroid, .06f);
        const auto rings = FrontierWorld::Create<ParticleObject>(app);
        if (auto *emitter = app.GetEntityComponent<ParticleEmitter>(rings)) {
          emitter->shape = ParticleShape::Box;
          emitter->extent = {.45f, .0f, .45f};
          emitter->capacity = 64;
          emitter->rate = 3.f;
          emitter->lifetime = 2.2f;
          emitter->lifetimeVariance = .2f;
          emitter->speed = .0f;
          emitter->gravity = {};
          emitter->facing = ParticleFacing::Flat;
          emitter->sprite = ParticleSprite::Ring;
          emitter->startSize = .05f;
          emitter->endSize = .7f;
          emitter->startColor = {.8f, .95f, 1.f};
          emitter->endColor = {.5f, .7f, .8f};
          emitter->intensity = .9f;
        }
        if (auto *transform = app.GetEntityComponent<Transform>(rings))
          transform->position = at;
        app.MarkTransformDirty(rings);
        entities.push_back(rings);
        break;
      }
      default:
        break;
      }
    }
  }
  // Weather the wind carries over the whole map: leaves torn from the woods, pollen over the meadows,
  // and spray thrown up where rock meets the sea.
  const auto low = grid.GetMin();
  const auto high = grid.GetMax();
  const auto middle = (low + high) * .5f;
  const auto half = (high - low) * .5f;
  const auto Weather = [&](const glm::vec3 &at, auto &&configure) {
    const auto id = FrontierWorld::Create<ParticleObject>(app);
    if (auto *emitter = app.GetEntityComponent<ParticleEmitter>(id))
      configure(*emitter);
    if (auto *transform = app.GetEntityComponent<Transform>(id))
      transform->position = at;
    app.MarkTransformDirty(id);
    entities.push_back(id);
  };
  Weather(ToWorld(middle, 3.5f), [&](ParticleEmitter &leaves) {
    leaves.shape = ParticleShape::Box;
    leaves.extent = {half.x, 1.5f, half.y};
    leaves.capacity = 1536;
    leaves.rate = 110.f;
    leaves.lifetime = 12.f;
    leaves.speed = .2f;
    leaves.spread = 180.f;
    leaves.gravity = {.0f, -.35f, .0f};
    leaves.drag = 1.f;
    leaves.turbulence = 2.2f;
    leaves.blend = ParticleBlend::Opaque;
    leaves.sprite = ParticleSprite::Petal;
    leaves.startSize = .09f;
    leaves.endSize = .08f;
    leaves.startColor = {.45f, .6f, .12f};
    leaves.endColor = {.75f, .5f, .1f};
    leaves.intensity = .7f;
  });
  Weather(ToWorld(middle, 1.2f), [&](ParticleEmitter &pollen) {
    pollen.shape = ParticleShape::Box;
    pollen.extent = {half.x, .8f, half.y};
    pollen.capacity = 2048;
    pollen.rate = 160.f;
    pollen.lifetime = 10.f;
    pollen.speed = .05f;
    pollen.spread = 180.f;
    pollen.gravity = {.0f, .03f, .0f};
    pollen.drag = .8f;
    pollen.turbulence = .7f;
    pollen.startSize = .03f;
    pollen.endSize = .02f;
    pollen.startColor = {1.f, .9f, .55f};
    pollen.endColor = {1.f, .75f, .4f};
    pollen.intensity = 2.2f;
  });
  auto sprays = 0;
  for (const auto &edge : grid.GetEdges()) {
    if (edge.right == INVALID_CELL || sprays >= 10)
      continue;
    const auto a = map.terrain[edge.left];
    const auto b = map.terrain[edge.right];
    const auto cliff = (a == Terrain::Rock && IsWater(b)) || (b == Terrain::Rock && IsWater(a));
    if (!cliff || !random.Chance(.3f))
      continue;
    ++sprays;
    const auto at = (grid.GetCorner(edge.from) + grid.GetCorner(edge.to)) * .5f;
    Weather(ToWorld(at, .05f), [&](ParticleEmitter &spray) {
      spray.capacity = 256;
      spray.rate = 40.f;
      spray.radius = .3f;
      spray.lifetime = 1.4f;
      spray.speed = 1.6f;
      spray.spread = 35.f;
      spray.gravity = {.0f, -3.f, .0f};
      spray.drag = 1.2f;
      spray.turbulence = .6f;
      spray.blend = ParticleBlend::Alpha;
      spray.opacity = .5f;
      spray.startSize = .12f;
      spray.endSize = .45f;
      spray.startColor = {.9f, .95f, 1.f};
      spray.endColor = {.8f, .88f, .95f};
      spray.intensity = 1.f;
    });
  }
}
auto MapSystem::Highlight(const std::span<const CellIndex> cells, const glm::vec3 &colour) -> void {
  if (std::ranges::equal(cells, highlighted) && colour == highlightColour)
    return;
  highlighted.assign(cells.begin(), cells.end());
  highlightColour = colour;
  if (auto *material = app.GetAsset<MaterialAsset>(MaterialId("highlight")))
    material->fallback.albedo = glm::vec4(colour, .9f);
  MeshBuilder mesh;
  const auto &grid = map.grid;
  for (const auto cell : cells) {
    if (cell >= grid.CellCount())
      continue;
    const auto &shape = grid.GetCell(cell);
    // Over the waves on the sea, which heave a little above nought, and just over the ground on land.
    const auto water = IsWater(map.terrain[cell]);
    for (size_t i = 0; i < shape.corners.size(); ++i) {
      const auto j = (i + 1) % shape.corners.size();
      const auto p = grid.GetCorner(shape.corners[i]);
      const auto q = grid.GetCorner(shape.corners[j]);
      const auto [hp, hq] = map.SideHeights(cell, i);
      const auto yp = water ? .12f : hp + .06f;
      const auto yq = water ? .12f : hq + .06f;
      const auto ip = p + (shape.centroid - p) * .16f;
      const auto iq = q + (shape.centroid - q) * .16f;
      mesh.OrientedQuad(ToWorld(p, yp), ToWorld(q, yq), ToWorld(iq, yq), ToWorld(ip, yp), UP);
    }
  }
  AddMesh(app, "highlight", mesh.Take());
}
auto MapSystem::BuildLayer() -> void {
  layersDirty = false;
  const auto &grid = map.grid;
  std::array<MeshBuilder, LAYER_CLASSES> meshes;
  std::array<glm::vec4, LAYER_CLASSES> colours{};
  // Which class each cell is drawn in for the layer showing, or none.
  const auto Classify = [&](const CellIndex c) -> int {
    switch (layer) {
    case Layer::Cells:
      return 0;
    case Layer::Resources:
      if (map.resource[c] < 0)
        return -1;
      return static_cast<int>(map.sites[static_cast<size_t>(map.resource[c])].kind) - 1;
    case Layer::Movement: {
      if (IsWater(map.terrain[c]))
        return -1;
      const auto cost = map.MoveCost(c);
      return cost <= .0f ? 3 : (cost <= 1.1f ? 0 : (cost <= 1.5f ? 1 : 2));
    }
    case Layer::Building:
      if (IsWater(map.terrain[c]))
        return -1;
      return map.occupant[c] >= 0 ? 1 : (map.IsFree(c) ? 0 : 2);
    default:
      return -1;
    }
  };
  switch (layer) {
  case Layer::Cells:
    colours[0] = {.02f, .02f, .03f, .55f};
    break;
  case Layer::Resources:
    colours = {glm::vec4(.2f, .9f, .2f, .5f), glm::vec4(.7f, .7f, .75f, .55f), glm::vec4(.3f, .9f, 1.f, .55f), glm::vec4(.2f, .4f, 1.f, .55f), glm::vec4(1.f, .2f, .3f, .55f), {}};
    break;
  case Layer::Movement:
    colours = {glm::vec4(.2f, .9f, .3f, .4f), glm::vec4(.95f, .9f, .2f, .45f), glm::vec4(1.f, .5f, .1f, .45f), glm::vec4(.9f, .1f, .1f, .5f), {}, {}};
    break;
  case Layer::Building:
    colours = {glm::vec4(.2f, .95f, .4f, .4f), glm::vec4(.25f, .5f, 1.f, .55f), glm::vec4(.9f, .15f, .15f, .35f), {}, {}, {}};
    break;
  default:
    break;
  }
  if (layer != Layer::None)
    for (CellIndex c = 0; c < grid.CellCount(); ++c) {
      const auto k = Classify(c);
      if (k < 0)
        continue;
      const auto &shape = grid.GetCell(c);
      const auto water = IsWater(map.terrain[c]);
      const auto lift = .04f;
      const auto Height = [&](const size_t i) { return water ? .1f : map.SideHeights(c, i).first + lift; };
      if (layer == Layer::Cells) {
        // An outline: a thin band inside each side.
        for (size_t i = 0; i < shape.corners.size(); ++i) {
          const auto j = (i + 1) % shape.corners.size();
          const auto p = grid.GetCorner(shape.corners[i]);
          const auto q = grid.GetCorner(shape.corners[j]);
          const auto ip = p + (shape.centroid - p) * .07f;
          const auto iq = q + (shape.centroid - q) * .07f;
          meshes[0].OrientedQuad(ToWorld(p, Height(i)), ToWorld(q, Height(j)), ToWorld(iq, Height(j)), ToWorld(ip, Height(i)), UP);
        }
        continue;
      }
      std::vector<glm::vec3> ring;
      for (size_t i = 0; i < shape.corners.size(); ++i) {
        const auto p = grid.GetCorner(shape.corners[i]);
        ring.push_back(ToWorld(p + (shape.centroid - p) * .06f, Height(i)));
      }
      meshes[static_cast<size_t>(k)].Fan(ToWorld(shape.centroid, water ? .1f : map.height[c] + map.peak[c] + lift), ring, UP);
    }
  for (auto k = 0; k < LAYER_CLASSES; ++k) {
    if (auto *material = app.GetAsset<MaterialAsset>(MaterialId(std::format("layer/{}", k))))
      material->fallback.albedo = colours[static_cast<size_t>(k)];
    AddMesh(app, std::format("layer/{}", k), meshes[static_cast<size_t>(k)].Take());
  }
}
auto MapSystem::BuildPath() -> void {
  MeshBuilder ribbon;
  constexpr float HALF = .07f;
  const auto Ground = [&](const glm::vec2 &p) { return std::max(map.GroundAt(p), .0f) + .08f; };
  for (size_t i = 1; i < path.size(); ++i) {
    const auto a = path[i - 1];
    const auto b = path[i];
    const auto length = glm::distance(a, b);
    if (length < 1e-4f)
      continue;
    const auto along = (b - a) / length;
    const glm::vec2 side{-along.y * HALF, along.x * HALF};
    const auto steps = std::max(1, static_cast<int>(length / .3f));
    for (auto k = 0; k < steps; ++k) {
      const auto p = a + (b - a) * (static_cast<float>(k) / static_cast<float>(steps));
      const auto q = a + (b - a) * (static_cast<float>(k + 1) / static_cast<float>(steps));
      ribbon.OrientedQuad(ToWorld(p - side, Ground(p)), ToWorld(p + side, Ground(p)), ToWorld(q + side, Ground(q)), ToWorld(q - side, Ground(q)), UP);
    }
  }
  // A post at each end and at every turn.
  for (const auto &point : path)
    ribbon.Frustum(ToWorld(point, Ground(point) - .08f), .06f, .03f, .45f, 5);
  if (pathStart && path.empty())
    ribbon.Frustum(ToWorld(*pathStart, Ground(*pathStart) - .08f), .06f, .03f, .45f, 5);
  AddMesh(app, "path", ribbon.Take());
}
auto MapSystem::Raycast(const glm::vec3 &origin, const glm::vec3 &direction) const -> std::optional<glm::vec3> {
  // Marched in steps a fraction of a cell long, then halved to the surface. The sea is a surface too,
  // at nought: a cursor over the water is over the water, not over the seabed under it.
  const auto Surface = [&](const glm::vec3 &p) { return std::max(map.GroundAt({p.x, p.z}), .0f); };
  constexpr float STEP = .2f;
  auto previous = origin;
  for (auto t = STEP; t < 600.f; t += STEP) {
    const auto p = origin + direction * t;
    if (p.y < Surface(p)) {
      auto a = previous;
      auto b = p;
      for (auto i = 0; i < 12; ++i) {
        const auto m = (a + b) * .5f;
        (m.y < Surface(m) ? b : a) = m;
      }
      return b;
    }
    if (p.y < FLOOR)
      break;
    previous = p;
  }
  return std::nullopt;
}
auto MapSystem::Update(const float) -> void {
  if (app.IsInputPressed(GLFW_KEY_G)) {
    seed = seed * 6364136223846793005ull % 1000003ull + 1u;
    Generate();
  }
  if (app.IsInputPressed(GLFW_KEY_TAB)) {
    layer = static_cast<Layer>((static_cast<size_t>(layer) + 1) % LAYER_NAMES.size());
    layersDirty = true;
  }
  if (auto *frontier = dynamic_cast<Frontier *>(&app)) {
    auto *camera = frontier->GetSystem<CameraSystem>();
    const auto [origin, direction] = camera->CursorRay();
    cursor = Raycast(origin, direction);
    hovered = cursor ? map.grid.Locate({cursor->x, cursor->z}) : INVALID_CELL;
    const auto focus = camera->GetFocus();
    camera->SetFocusHeight(map.GroundAt({focus.x, focus.z}));
  }
  if (pathEnabled && app.IsInputPressed(GLFW_MOUSE_BUTTON_RIGHT) && cursor) {
    const glm::vec2 point{cursor->x, cursor->z};
    if (!pathStart || !path.empty()) {
      pathStart = point;
      path.clear();
      pathLength = .0f;
    } else {
      path = map.nav.FindPath(*pathStart, point, .12f);
      pathLength = .0f;
      for (size_t i = 1; i < path.size(); ++i)
        pathLength += glm::distance(path[i - 1], path[i]);
      if (path.empty())
        pathStart.reset();
    }
    BuildPath();
  }
  if (layersDirty)
    BuildLayer();
  DrawPanel();
}
auto MapSystem::DrawPanel() -> void {
  const auto *context = app.GetGraphicsContext();
  if (!context)
    return;
  const auto [width, height] = context->GetSurfaceSize();
  if (width <= 0 || height <= 0)
    return;
  const auto unit = static_cast<float>(height) / 1080.f;
  auto &overlay = app.GetOverlay();
  const auto text = Display({1.f, .97f, .9f});
  const auto dim = Display({.75f, .78f, .8f}, .9f);
  const auto [low, high] = map.grid.NeighbourRange();
  overlay.DrawRect({18.f * unit, 18.f * unit}, {430.f * unit, 150.f * unit}, {.02f, .03f, .05f, .55f});
  overlay.DrawText("FRONTIER", {32.f * unit, 26.f * unit}, 34.f * unit, text);
  overlay.DrawText(std::format("Seed {}   {} cells, {}-{} neighbours", map.seed, map.CellCount(), low, high), {32.f * unit, 70.f * unit}, 19.f * unit, dim);
  overlay.DrawText(std::format("Collapse: {} repairs, {} attempt{}, {:.0f} ms", map.repairs, map.attempts, map.attempts == 1 ? "" : "s", map.elapsed * 1000.f), {32.f * unit, 96.f * unit}, 19.f * unit, dim);
  overlay.DrawText(std::format("Layer: {}  (Tab)", LAYER_NAMES[static_cast<size_t>(layer)]), {32.f * unit, 122.f * unit}, 19.f * unit, dim);
  if (hovered != INVALID_CELL) {
    const auto &cell = map.grid.GetCell(hovered);
    std::string resource = "none";
    if (const auto site = map.resource[hovered]; site >= 0) {
      const auto &s = map.sites[static_cast<size_t>(site)];
      resource = std::format("{} ({}, {} cell{})", RESOURCE_NAMES[static_cast<size_t>(s.kind)], s.amount, s.cells.size(), s.cells.size() == 1 ? "" : "s");
    }
    const auto cost = map.MoveCost(hovered);
    overlay.DrawRect({18.f * unit, 180.f * unit}, {430.f * unit, 150.f * unit}, {.02f, .03f, .05f, .55f});
    overlay.DrawText(std::format("Cell {}: {}", hovered, Info(map.terrain[hovered]).name), {32.f * unit, 190.f * unit}, 22.f * unit, text);
    overlay.DrawText(std::format("{} neighbours, {:.2f} high", cell.neighbours.size(), map.height[hovered] + map.peak[hovered]), {32.f * unit, 222.f * unit}, 19.f * unit, dim);
    overlay.DrawText(std::format("Resource: {}", resource), {32.f * unit, 248.f * unit}, 19.f * unit, dim);
    overlay.DrawText(cost > .0f ? std::format("Movement: {:.1f}x", cost) : std::string("Movement: blocked"), {32.f * unit, 274.f * unit}, 19.f * unit, dim);
    overlay.DrawText(map.occupant[hovered] >= 0 ? std::string("Building: occupied") : (map.IsFree(hovered) ? "Building: free" : "Building: not here"), {32.f * unit, 300.f * unit}, 19.f * unit, dim);
  }
  if (!path.empty())
    overlay.DrawText(std::format("Path: {} turns, {:.1f} long", path.size() - 2, pathLength), {0.f, 24.f * unit}, 22.f * unit, text, TextAnchor::Top);
  else if (pathStart)
    overlay.DrawText("Right click where the path should end", {0.f, 24.f * unit}, 22.f * unit, text, TextAnchor::Top);
  overlay.DrawText("WASD move   wheel zoom   Q/E or middle drag turn   1-7 build   Tab layer   right click path   G new map   T time   Esc quit", {.0f, -18.f * unit}, 18.f * unit, dim, TextAnchor::Bottom);
}
