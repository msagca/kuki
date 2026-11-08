#include <algorithm>
#include <application.hpp>
#include <application_internal.hpp>
#include <cmath>
#include <engine_archetypes.hpp>
#include <fstream>
#include <glm/ext/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <graphics_context.hpp>
#include <light.hpp>
#include <light_type.hpp>
#include <material_asset.hpp>
#include <mesh_asset.hpp>
#include <mesh_builder.hpp>
#include <model_asset.hpp>
#include <nlohmann/json.hpp>
#include <numbers>
#include <particle_emitter.hpp>
#include <particle_presets.hpp>
#include <pool.hpp>
#include <profiler.hpp>
#include <random.hpp>
#include <scene.hpp>
#include <spdlog/spdlog.h>
#include <stress_system.hpp>
#include <transform.hpp>
#include <world.hpp>
namespace kuki {
namespace {
  /// @brief The entity a load hangs from, by which it is told from anything else in the scene.
  constexpr auto ROOT_NAME = "StressRoot";
  constexpr auto FIGURE_NAME = "StressFigure";
  constexpr auto MATERIAL_COUNT = 8u;
  /// @brief The figure: a column of bones, each a tapering six-sided segment of the body, swaying.
  ///
  /// Built here rather than loaded, because the editor ships no rigged model and a stress test that
  /// needed one from a game would only run in that game. Rigidly skinned -- each vertex to the one
  /// bone of its segment -- which costs the shader what a blend of four does, since it reads four
  /// weights either way.
  constexpr auto FIGURE_BONES = 6u;
  constexpr auto FIGURE_SEGMENT = .4f;
  constexpr auto FIGURE_CLIP_TICKS = 48.f;
  constexpr auto FIGURE_CLIP_RATE = 24.f;
  /// @brief Seeded once, so two runs of one load place the same things in the same places and their
  /// numbers can be compared.
  constexpr uint64_t SEED = 0x5713e55ull;
  /// @brief The share of a step's frame a scope must take to be reported, and how many are.
  constexpr auto SCOPE_THRESHOLD_MILLIS = .05;
  constexpr auto REPORTED_SCOPES = 12u;
  /// @brief A colour of the rainbow, `hue` from 0 to 1, for telling apart what is otherwise the same.
  auto Hue(const float hue) -> glm::vec3 {
    const auto Channel = [&](const float offset) {
      return .5f + .5f * std::cos(2.f * std::numbers::pi_v<float> * (hue + offset));
    };
    return {Channel(.0f), Channel(2.f / 3.f), Channel(1.f / 3.f)};
  }
  auto BuildFigure(const AssetID id) -> std::unique_ptr<ModelAsset> {
    auto model = std::make_unique<ModelAsset>(id);
    MeshBuilder builder;
    std::array<size_t, FIGURE_BONES + 1> firstVertex{};
    const auto Radius = [](const uint32_t i) {
      return .22f - .14f * static_cast<float>(i) / FIGURE_BONES;
    };
    for (auto i = 0u; i < FIGURE_BONES; ++i) {
      firstVertex[i] = builder.VertexCount();
      builder.Frustum({.0f, static_cast<float>(i) * FIGURE_SEGMENT, .0f}, Radius(i), Radius(i + 1), FIGURE_SEGMENT, 6);
    }
    // The head, on the last bone with the last segment.
    builder.Block({.0f, FIGURE_BONES * FIGURE_SEGMENT + .1f, .0f}, glm::vec3(.12f));
    firstVertex[FIGURE_BONES] = builder.VertexCount();
    auto mesh = builder.Take();
    for (size_t vertex = 0, bone = 0; vertex < mesh.vertices.size(); ++vertex) {
      while (bone + 1 < FIGURE_BONES && vertex >= firstVertex[bone + 1])
        ++bone;
      mesh.vertices[vertex].boneIds = glm::ivec4(static_cast<int>(bone), 0, 0, 0);
      mesh.vertices[vertex].boneWeights = glm::vec4(1.f, .0f, .0f, .0f);
    }
    // Node 0 is the root, 1 holds the mesh, and 2 on are the bones, each hanging from the one before.
    constexpr auto FIRST_BONE = 2u;
    model->nodes.resize(FIRST_BONE + FIGURE_BONES);
    model->nodes[0] = {.transform = glm::mat4(1.f), .parent = -1, .name = FIGURE_NAME, .children = {1, FIRST_BONE}};
    model->nodes[1] = {.transform = glm::mat4(1.f), .parent = 0, .name = std::string(FIGURE_NAME) + "Body", .meshes = {0}};
    ModelMesh body{.mesh = std::move(mesh), .name = "Body", .material = 0, .parent = 1};
    AnimationClip clip{.name = "Sway", .duration = FIGURE_CLIP_TICKS, .ticksPerSecond = FIGURE_CLIP_RATE};
    for (auto i = 0u; i < FIGURE_BONES; ++i) {
      const auto node = FIRST_BONE + i;
      auto &bone = model->nodes[node];
      bone.name = "StressBone" + std::to_string(i);
      bone.parent = static_cast<int>(i == 0 ? 0 : node - 1);
      bone.transform = glm::translate(glm::mat4(1.f), {.0f, i == 0 ? .0f : FIGURE_SEGMENT, .0f});
      if (i + 1 < FIGURE_BONES)
        bone.children = {node + 1};
      // The inverse of where the bone stands in the model at rest, which is where the segment was built.
      body.bones.push_back({.name = bone.name, .nodeIndex = node, .offsetMatrix = glm::translate(glm::mat4(1.f), {.0f, -static_cast<float>(i) * FIGURE_SEGMENT, .0f})});
      // Each bone a little behind the one below it, so the sway travels up the body as a wave.
      AnimationChannel channel{.nodeIndex = node, .nodeName = bone.name};
      constexpr auto KEYS = 8;
      for (auto key = 0; key <= KEYS; ++key) {
        const auto phase = 2.f * std::numbers::pi_v<float> * static_cast<float>(key) / KEYS + .7f * static_cast<float>(i);
        channel.rotations.push_back({.time = FIGURE_CLIP_TICKS * key / KEYS, .value = glm::quat(glm::vec3(.2f * std::sin(phase), .0f, .2f * std::cos(phase)))});
      }
      clip.channels.push_back(std::move(channel));
    }
    model->meshes.push_back(std::move(body));
    ModelMaterial material{.type = MaterialType::Lit, .name = FIGURE_NAME};
    material.fallback.albedo = {.85f, .55f, .3f, 1.f};
    model->materials.push_back(std::move(material));
    model->animations.push_back(std::move(clip));
    return model;
  }
  auto Scaled(const StressLoad &load, const float factor) -> StressLoad {
    const auto Scale = [&](const uint32_t count) {
      return static_cast<uint32_t>(std::lround(static_cast<double>(count) * factor));
    };
    auto scaled = load;
    scaled.instances = Scale(load.instances);
    scaled.rigs = Scale(load.rigs);
    scaled.lights = Scale(load.lights);
    scaled.emitters = Scale(load.emitters);
    return scaled;
  }
  auto ParseFloat(const std::optional<std::string> &text, const Flag &flag, const float fallback) -> float {
    if (!text)
      return fallback;
    try {
      if (const auto value = std::stof(*text); value > .0f)
        return value;
    } catch (...) {
    }
    spdlog::warn("[Stress] {} wants a number above nought, not {}; using {}", flag.name, *text, fallback);
    return fallback;
  }
} // namespace
auto FindStressPreset(const std::string_view name) -> std::optional<StressLoad> {
  for (const auto &preset : STRESS_PRESETS)
    if (preset.name == name)
      return preset.load;
  return std::nullopt;
}
StressSystem::StressSystem(Application &app)
  : System(std::in_place_type<StressSystem>, app) {}
auto StressSystem::Start() -> void {
  LoadAssets();
  steps.reserve(STRESS_STEPS);
  scopeTotals.reserve(256);
}
auto StressSystem::LoadAssets() -> void {
  for (const auto *name : {"Cube", "Sphere", "Cylinder"})
    if (const auto *asset = app.GetAsset(std::string(name)); asset)
      meshes.push_back(asset->id);
  // A low-poly rock, the kind of thing a game scatters by the hundred.
  {
    const auto id = MakeBuiltInAssetID("stress/rock");
    MeshBuilder builder;
    builder.Rock({}, glm::vec3(.5f), .35f, 7, 1);
    auto rock = std::make_unique<MeshAsset>(id);
    rock->bounds = builder.Bounds();
    rock->mesh = builder.Take();
    app.AddAsset(std::move(rock), "StressRock");
    meshes.push_back(id);
  }
  for (auto i = 0u; i < MATERIAL_COUNT; ++i) {
    const auto id = MakeBuiltInAssetID("stress/material/" + std::to_string(i));
    auto material = std::make_unique<MaterialAsset>(id);
    material->type = MaterialType::Lit;
    material->fallback.albedo = glm::vec4(Hue(static_cast<float>(i) / MATERIAL_COUNT), 1.f);
    app.AddAsset(std::move(material), "StressMaterial" + std::to_string(i));
    materials.push_back(id);
  }
  figure = MakeBuiltInAssetID("stress/figure");
  app.AddAsset(BuildFigure(figure), FIGURE_NAME);
}
auto StressSystem::PostStart() -> void {
  const auto arguments = GetCommandLineArguments();
  const auto name = GetFlagValue(arguments, STRESS_FLAG);
  if (!name)
    return;
  const auto preset = FindStressPreset(*name);
  if (!preset) {
    std::string names;
    for (const auto &each : STRESS_PRESETS)
      names += (names.empty() ? "" : ", ") + std::string(each.name);
    spdlog::error("[Stress] There is no load called {}; there are {}", *name, names);
    Internal(app).SetExitCode(2);
    Internal(app).Quit();
    return;
  }
  budget = ParseFloat(GetFlagValue(arguments, STRESS_BUDGET_FLAG), STRESS_BUDGET_FLAG, budget);
  const auto scale = ParseFloat(GetFlagValue(arguments, STRESS_SCALE_FLAG), STRESS_SCALE_FLAG, 1.f);
  if (const auto path = GetFlagValue(arguments, STRESS_REPORT_FLAG))
    reportPath = *path;
  fromCommandLine = true;
  spdlog::info("[Stress] Ramping up to {} at {}x in {} steps of {} frames, against {:.2f} ms", *name, scale, STRESS_STEPS, MEASURE_FRAMES, budget);
  BeginRun(Scaled(*preset, scale));
}
auto StressSystem::Update(const float deltaTime) -> void {
  KUKI_PROFILE_SCOPE("Stress");
  const auto millis = deltaTime * 1000.f;
  recent[recentOffset] = millis;
  recentOffset = (recentOffset + 1) % RECENT_FRAMES;
  clock += deltaTime;
  if (load.moving && load.instances > 0 && HasRoot()) {
    // Found through the transforms' parent rather than a list of ids kept from when they were made:
    // a scene opened or put back since may have given those ids to entities that are not these.
    const auto time = clock;
    EngineWorld::ForEach<Transform, Pooled>(app, [&](const EntityID id, Transform *transform, Pooled *) {
      if (transform->parent != root)
        return;
      const auto seed = static_cast<float>(static_cast<long long>(id) % 97);
      const auto axis = glm::normalize(glm::vec3(std::sin(seed), 1.f, std::cos(seed)));
      transform->rotation = glm::angleAxis(time * (.4f + .02f * seed), axis);
      app.MarkTransformDirty(id);
    });
  }
  switch (phase) {
  case StressPhase::Idle:
    break;
  case StressPhase::Settling:
    if (++phaseFrames >= SETTLE_FRAMES) {
      // From here on a node's total is compared with what it is now. Frames are folded into the
      // totals at their end, so the step's own frames are this one and the next `MEASURE_FRAMES - 1`,
      // and each frame's time is read the frame after it, as `deltaTime` -- the two cover the same frames.
      const auto &nodes = Profiler::Get().GetNodes();
      scopeTotals.resize(nodes.size());
      for (size_t i = 0; i < nodes.size(); ++i)
        scopeTotals[i] = nodes[i].totalMillis;
      phase = StressPhase::Measuring;
      phaseFrames = 0;
    }
    break;
  case StressPhase::Measuring:
    samples[phaseFrames++] = millis;
    if (phaseFrames >= MEASURE_FRAMES)
      EndStep();
    break;
  }
}
auto StressSystem::Shutdown() -> void {
  if (phase != StressPhase::Idle)
    CancelRun();
}
auto StressSystem::HasRoot() -> bool {
  if (root && app.IsEntity(root) && Internal(app).GetEntityName(root) == ROOT_NAME)
    return true;
  // Found again by its name, which a scene keeps when it is saved and opened where it may not keep
  // the id: the editor's Stop puts back a copy of the scene, load and all.
  const auto *scene = Internal(app).Scenes().Get();
  root = scene ? scene->Entities().GetID(ROOT_NAME) : EntityID{};
  return static_cast<bool>(root);
}
auto StressSystem::SetLoad(const StressLoad &requested) -> void {
  Clear();
  load = requested;
  const auto maxPools = static_cast<uint32_t>(meshes.size() * materials.size());
  load.pools = std::clamp(load.pools, 1u, std::max(1u, maxPools));
  if (load.instances + load.rigs + load.lights + load.emitters == 0)
    return;
  if (load.lights / 2 > 8)
    spdlog::warn("[Stress] {} lights asked for; the renderer shades eight point lights and eight spot lights, and draws nothing of the rest", load.lights);
  // In a box that grows with the instances so they are as crowded at any count, its near face a
  // step in front of the camera. Not far enough off to be seen whole, which was tried: the editor's
  // own scene is a closed room, and a box far enough away to fit the view stood behind its back
  // wall, where the depth test threw away nearly all it cost and the load measured almost nothing.
  const auto extent = std::max(6.f, .6f * std::cbrt(static_cast<float>(load.instances)));
  auto centre = glm::vec3(.0f, extent, .0f);
  if (const auto *camera = app.GetEntityComponent<WorldMatrix>(app.GetCameraEntity()); camera)
    centre = WorldPosition(camera->matrix) + WorldForward(camera->matrix) * (extent + 1.f);
  root = EngineWorld::Create<EmptyNode>(app, ROOT_NAME);
  if (!root)
    return;
  app.GetEntityComponent<Transform>(root)->position = centre;
  app.MarkTransformDirty(root);
  Random random(SEED);
  const auto Inside = [&] {
    return glm::vec3(random.Range(-extent, extent), random.Range(-extent, extent), random.Range(-extent, extent));
  };
  // Placed under the root, where it is.
  const auto Place = [&](const EntityID id, const glm::vec3 &position, const glm::quat &rotation = glm::quat(1.f, .0f, .0f, .0f), const glm::vec3 &scale = glm::vec3(1.f)) {
    app.AddChildEntity(root, id);
    auto *transform = app.GetEntityComponent<Transform>(id);
    transform->position = position;
    transform->rotation = rotation;
    transform->scale = scale;
    app.MarkTransformDirty(id);
  };
  // One pool after another rather than one of each in turn, so that a pool is filled in one go.
  std::vector<PoolKey> pools(load.pools);
  for (uint32_t p = 0; p < load.pools; ++p) {
    pools[p] = {.mesh = meshes[p % meshes.size()], .material = materials[(p / meshes.size()) % materials.size()]};
    EngineWorld::CreatePool<MeshObject>(app, pools[p], {.capacity = load.instances / load.pools + 1});
  }
  for (uint32_t i = 0; i < load.instances; ++i) {
    const auto id = EngineWorld::Create<MeshObject>(app, pools[static_cast<uint64_t>(i) * load.pools / load.instances]);
    const auto turn = glm::angleAxis(random.Range(.0f, 2.f * std::numbers::pi_v<float>), glm::normalize(Inside() + glm::vec3(.0f, .0f, .001f)));
    Place(id, Inside(), turn, glm::vec3(random.Range(.3f, .6f)));
  }
  // Standing in rows across the box a little below the eye, where they are seen -- the floor of a
  // box this deep is out of the bottom of the view -- each a different way round and at a
  // different point of its clip.
  constexpr auto RIG_HEIGHT = -2.f;
  const auto side = static_cast<uint32_t>(std::ceil(std::sqrt(static_cast<float>(load.rigs))));
  for (uint32_t i = 0; i < load.rigs; ++i) {
    const auto id = EngineWorld::Instantiate<ModelRoot>(app, FIGURE_NAME);
    if (!id)
      break;
    const auto spacing = 2.f * extent / static_cast<float>(side);
    const glm::vec3 position{-extent + spacing * (static_cast<float>(i % side) + .5f), RIG_HEIGHT, -extent + spacing * (static_cast<float>(i / side) + .5f)};
    Place(id, position, glm::angleAxis(random.Range(.0f, 2.f * std::numbers::pi_v<float>), glm::vec3(.0f, 1.f, .0f)));
    if (auto *animator = app.GetEntityComponent<Animator>(id); animator)
      animator->time = random.Range(.0f, FIGURE_CLIP_TICKS);
  }
  // Point lights inside the box, spot lights above it shining down into it, alternately.
  for (uint32_t i = 0; i < load.lights; ++i) {
    const auto id = EngineWorld::Create<LightObject>(app);
    auto *light = app.GetEntityComponent<Light>(id);
    light->diffuse = Hue(static_cast<float>(i) / static_cast<float>(std::max(1u, load.lights)));
    light->specular = light->diffuse;
    light->ambient = glm::vec3(.0f);
    if (i % 2 == 0) {
      light->type = LightType::Point;
      light->intensity = 2.f;
      Place(id, Inside());
    } else {
      light->type = LightType::Spot;
      light->intensity = 4.f;
      light->farPlane = 4.f * extent + 20.f;
      const glm::vec3 position{random.Range(-extent, extent), extent + 4.f, random.Range(-extent, extent)};
      const glm::vec3 target{random.Range(-extent, extent) * .5f, -extent, random.Range(-extent, extent) * .5f};
      Place(id, position, LookRotation(target - position));
    }
  }
  for (uint32_t i = 0; i < load.emitters; ++i) {
    const auto id = EngineWorld::Create<ParticleObject>(app);
    auto *emitter = app.GetEntityComponent<ParticleEmitter>(id);
    *emitter = ParticlePresets::Fountain();
    emitter->startColor = Hue(static_cast<float>(i) / static_cast<float>(std::max(1u, load.emitters)));
    Place(id, Inside());
  }
}
auto StressSystem::GetLoad() const -> const StressLoad & {
  return load;
}
auto StressSystem::Clear() -> void {
  if (HasRoot())
    app.DeleteEntity(root);
  root = {};
  load = {};
}
auto StressSystem::BeginRun(const StressLoad &requested) -> void {
  if (phase != StressPhase::Idle)
    CancelRun();
  target = requested;
  steps.clear();
  passed = false;
  stepIndex = 0;
  auto &profiler = Profiler::Get();
  savedProfiling = profiler.IsEnabled();
  savedPaused = profiler.IsPaused();
  profiler.SetEnabled(true);
  profiler.SetPaused(false);
  if (auto *context = Internal(app).GetGraphicsContext(); context) {
    savedVSync = context->GetVSync();
    context->SetVSync(false);
  }
  BeginStep();
}
auto StressSystem::BeginStep() -> void {
  SetLoad(Scaled(target, static_cast<float>(stepIndex + 1) / STRESS_STEPS));
  phase = StressPhase::Settling;
  phaseFrames = 0;
}
auto StressSystem::EndStep() -> void {
  StressStep step{.load = load, .frames = MEASURE_FRAMES};
  auto sorted = samples;
  std::ranges::sort(sorted);
  double total{};
  for (const auto sample : sorted)
    total += sample;
  step.averageMillis = static_cast<float>(total / MEASURE_FRAMES);
  step.p95Millis = sorted[std::min<size_t>(MEASURE_FRAMES - 1, MEASURE_FRAMES * 95 / 100)];
  step.maxMillis = sorted.back();
  step.withinBudget = step.p95Millis <= budget;
  const auto &nodes = Profiler::Get().GetNodes();
  for (size_t i = 0; i < nodes.size(); ++i) {
    const auto millis = (nodes[i].totalMillis - (i < scopeTotals.size() ? scopeTotals[i] : .0)) / MEASURE_FRAMES;
    if (millis < SCOPE_THRESHOLD_MILLIS)
      continue;
    std::string path = nodes[i].name;
    for (auto parent = nodes[i].parent; parent != PROFILE_ROOT; parent = nodes[parent].parent)
      path = nodes[parent].name + "/" + path;
    step.scopes.push_back({std::move(path), millis});
  }
  std::ranges::sort(step.scopes, std::greater{}, &StressScope::millis);
  if (step.scopes.size() > REPORTED_SCOPES)
    step.scopes.resize(REPORTED_SCOPES);
  LogStep(step);
  steps.push_back(std::move(step));
  if (++stepIndex < STRESS_STEPS)
    BeginStep();
  else
    EndRun();
}
auto StressSystem::LogStep(const StressStep &step) const -> void {
  const auto &l = step.load;
  spdlog::info("[Stress] Step {}/{}: {} instances in {} pools, {} rigs, {} lights, {} emitters -- {:.2f} ms average, {:.2f} p95, {:.2f} max: {}", steps.size() + 1, STRESS_STEPS, l.instances, l.pools, l.rigs, l.lights, l.emitters, step.averageMillis, step.p95Millis, step.maxMillis, step.withinBudget ? "within budget" : "OVER BUDGET");
  for (size_t i = 0; i < step.scopes.size() && i < 6; ++i)
    spdlog::info("[Stress]   {:7.3f} ms  {}", step.scopes[i].millis, step.scopes[i].path);
}
auto StressSystem::EndRun() -> void {
  passed = steps.size() == STRESS_STEPS && steps.back().withinBudget;
  CancelRun();
  if (!fromCommandLine)
    return;
  // Asked for once: a ramp begun from a panel afterwards is not the command line's.
  fromCommandLine = false;
  spdlog::info("[Stress] {}: {:.2f} ms p95 at the full load, against {:.2f}", passed ? "Passed" : "Failed", steps.empty() ? .0f : steps.back().p95Millis, budget);
  if (!reportPath.empty()) {
    if (WriteReport(reportPath))
      spdlog::info("[Stress] Wrote {}", reportPath.string());
    else
      spdlog::error("[Stress] Could not write {}", reportPath.string());
  }
  Internal(app).SetExitCode(passed ? 0 : 1);
  Internal(app).Quit();
}
auto StressSystem::CancelRun() -> void {
  if (phase == StressPhase::Idle)
    return;
  phase = StressPhase::Idle;
  Clear();
  auto &profiler = Profiler::Get();
  profiler.SetPaused(savedPaused);
  profiler.SetEnabled(savedProfiling);
  if (auto *context = Internal(app).GetGraphicsContext(); context)
    context->SetVSync(savedVSync);
}
auto StressSystem::GetPhase() const -> StressPhase {
  return phase;
}
auto StressSystem::GetStepIndex() const -> uint32_t {
  return stepIndex;
}
auto StressSystem::GetPhaseFrames() const -> uint32_t {
  return phaseFrames;
}
auto StressSystem::GetSteps() const -> const std::vector<StressStep> & {
  return steps;
}
auto StressSystem::GetBudget() const -> float {
  return budget;
}
auto StressSystem::SetBudget(const float millis) -> void {
  budget = std::max(.1f, millis);
}
auto StressSystem::Passed() const -> bool {
  return passed;
}
auto StressSystem::WriteReport(const std::filesystem::path &path) const -> bool {
  auto json = nlohmann::json::object();
  json["budgetMillis"] = budget;
  json["passed"] = passed;
  json["settleFrames"] = SETTLE_FRAMES;
  auto &list = json["steps"] = nlohmann::json::array();
  for (const auto &step : steps) {
    auto &entry = list.emplace_back();
    entry["instances"] = step.load.instances;
    entry["pools"] = step.load.pools;
    entry["rigs"] = step.load.rigs;
    entry["lights"] = step.load.lights;
    entry["emitters"] = step.load.emitters;
    entry["moving"] = step.load.moving;
    entry["frames"] = step.frames;
    entry["averageMillis"] = step.averageMillis;
    entry["p95Millis"] = step.p95Millis;
    entry["maxMillis"] = step.maxMillis;
    entry["withinBudget"] = step.withinBudget;
    auto &scopes = entry["scopes"] = nlohmann::json::array();
    for (const auto &scope : step.scopes)
      scopes.push_back({{"path", scope.path}, {"millis", scope.millis}});
  }
  std::ofstream file(path);
  if (!file)
    return false;
  file << json.dump(2) << '\n';
  return static_cast<bool>(file);
}
auto StressSystem::GetRecentFrames() const -> const std::array<float, RECENT_FRAMES> & {
  return recent;
}
auto StressSystem::GetRecentOffset() const -> size_t {
  return recentOffset;
}
} // namespace kuki
