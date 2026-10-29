#include <animator.hpp>
#include <application.hpp>
#include <atmosphere_sky.hpp>
#include <bounding_box.hpp>
#include <camera.hpp>
#include <component.hpp>
#include <fstream>
#include <inverse_kinematics.hpp>
#include <launch_path.hpp>
#include <light.hpp>
#include <material_asset.hpp>
#include <material_handle.hpp>
#include <mesh_handle.hpp>
#include <model_asset.hpp>
#include <optional>
#include <particle_emitter.hpp>
#include <scene_serializer.hpp>
#include <serdes.hpp>
#include <shader_asset.hpp>
#include <skybox_handle.hpp>
#include <spdlog/spdlog.h>
#include <texture_asset.hpp>
#include <texture_handle.hpp>
#include <transform.hpp>
#include <unordered_map>
#include <unordered_set>
#include <volumetric_clouds.hpp>
#include <stylization.hpp>
#include <volumetric_fog.hpp>
using json = nlohmann::json;
namespace kuki {
namespace {
  constexpr ComponentType SERIALIZED_TYPES[]{
    ComponentType::Animator,
    // The one settings component that travels. `IndirectLighting` and `AntiAliasing` deliberately
    // do not: they describe how somebody is looking at their work. An atmosphere describes the
    // work -- what time of day it is and how high up the scene sits -- so it belongs in the file
    // the way a light does.
    ComponentType::AtmosphereSky,
    ComponentType::BoundingBox,
    ComponentType::Camera,
    ComponentType::Light,
    ComponentType::MaterialHandle,
    ComponentType::MeshHandle,
    ComponentType::ModelMaterialHandle,
    ComponentType::ModelMeshHandle,
    // Travels, like a light does: where the sparks come from and what they look like is part of
    // the scene. What does not travel is the particles themselves, which live in a buffer on the
    // GPU that nothing reads back -- a loaded scene starts its emitters from empty.
    ComponentType::ParticleEmitter,
    ComponentType::SkyboxHandle,
    // Travels, as the atmosphere does: whether a scene is drawn in cel bands and ink is a decision
    // about the work. `ScreenEffects` does not -- it is what a game is doing to the picture this
    // instant, and a save that caught a flash would open on a white screen.
    ComponentType::Stylization,
    ComponentType::TextureHandle,
    ComponentType::Transform,
    // Both travel, on the same terms as the atmosphere above and for the same reason. How thick a
    // room's air is and how much cloud there is over the scene are facts about the work, not about
    // the desk it is being looked at on.
    ComponentType::VolumetricClouds,
    ComponentType::VolumetricFog,
  };
  auto IsSerialized(const ComponentType type) -> bool {
    for (const auto t : SERIALIZED_TYPES)
      if (t == type)
        return true;
    return false;
  }
  auto NameToType() -> const std::unordered_map<std::string, ComponentType> & {
    static const auto map = [] {
      std::unordered_map<std::string, ComponentType> m;
      for (const auto t : SERIALIZED_TYPES)
        m.emplace(Component::GetTypeName(t), t);
      return m;
    }();
    return map;
  }
  /// @brief Every component the application's archetypes have, by id and by name.
  ///
  /// From the archetypes and not from the engine's list, because a game's components are among them
  /// and the engine knows their names only from what the game's archetypes say.
  struct ComponentNames {
    std::unordered_map<ComponentId, std::string> byId;
    std::unordered_map<std::string, ComponentId> byName;
  };
  auto GetComponentNames(const Application &app) -> ComponentNames {
    ComponentNames names;
    for (const auto &spec : app.GetArchetypes())
      for (size_t index = 0; index < spec.ids.size(); ++index) {
        names.byId.emplace(spec.ids[index], std::string(spec.names[index]));
        names.byName.emplace(std::string(spec.names[index]), spec.ids[index]);
      }
    return names;
  }
  /// @brief The archetype an entity of a file written before archetypes were recorded was of.
  ///
  /// The file has only the components that travel. The archetype that has exactly those, of the
  /// ones that travel, is the answer when there is one. Several can: `{Transform}` is an empty node
  /// and also every setting that stays behind on its entity's transform, and the plainest of them --
  /// the one with the fewest components -- is taken, which is the empty node.
  ///
  /// Failing that, the plainest archetype that has all of them and more besides, which is what a
  /// file saved before every entity had a transform needs: a camera saved as nothing but a `Camera`
  /// comes back as a `CameraObject`, its transform at its default. What it cannot be is an archetype
  /// missing one of the components the file has values for, which would throw them away.
  auto GuessArchetype(const Application &app, const ComponentMask &saved) -> std::optional<ComponentMask> {
    ComponentMask travelling;
    for (const auto type : SERIALIZED_TYPES)
      travelling.set(static_cast<size_t>(type));
    std::optional<ComponentMask> exact, wider;
    for (const auto &spec : app.GetArchetypes()) {
      const auto kept = spec.mask & travelling;
      if (kept == saved && (!exact || spec.mask.count() < exact->count()))
        exact = spec.mask;
      else if ((kept & saved) == saved && (!wider || spec.mask.count() < wider->count()))
        wider = spec.mask;
    }
    return exact ? exact : wider;
  }
  auto Remap(const std::unordered_map<AssetID, AssetID> &remap, const AssetID id) -> AssetID {
    if (auto it = remap.find(id); it != remap.end())
      return it->second;
    return id;
  }
  auto ManifestPath(const std::filesystem::path &scenePath) -> std::filesystem::path {
    auto path = scenePath;
    path.replace_extension();
    path += ".assets.json";
    return path;
  }
  auto ToJson(const AtmosphereSky &a) -> json {
    return {
      {"bottomRadius", a.bottomRadius},
      {"atmosphereHeight", a.atmosphereHeight},
      {"rayleighScattering", a.rayleighScattering},
      {"rayleighScaleHeight", a.rayleighScaleHeight},
      {"mieScattering", a.mieScattering},
      {"mieAbsorption", a.mieAbsorption},
      {"mieScaleHeight", a.mieScaleHeight},
      {"mieAnisotropy", a.mieAnisotropy},
      {"ozoneAbsorption", a.ozoneAbsorption},
      {"ozoneCenter", a.ozoneCenter},
      {"ozoneWidth", a.ozoneWidth},
      {"viewAltitude", a.viewAltitude},
      {"sunIntensity", a.sunIntensity},
      {"multiscatterStrength", a.multiscatterStrength},
      {"groundAlbedo", a.groundAlbedo},
      {"marchSteps", a.marchSteps}};
  }
  auto FromJson(const json &j, AtmosphereSky &a) -> void {
    // Every field optional and defaulted from the struct, as the handlers above are. A scene saved
    // before a field existed has to keep loading, and the default is what that field meant before
    // anyone could write it down.
    const auto Read = [&j](const char *key, auto &field) {
      if (j.contains(key))
        j.at(key).get_to(field);
    };
    Read("bottomRadius", a.bottomRadius);
    Read("atmosphereHeight", a.atmosphereHeight);
    Read("rayleighScattering", a.rayleighScattering);
    Read("rayleighScaleHeight", a.rayleighScaleHeight);
    Read("mieScattering", a.mieScattering);
    Read("mieAbsorption", a.mieAbsorption);
    Read("mieScaleHeight", a.mieScaleHeight);
    Read("mieAnisotropy", a.mieAnisotropy);
    Read("ozoneAbsorption", a.ozoneAbsorption);
    Read("ozoneCenter", a.ozoneCenter);
    Read("ozoneWidth", a.ozoneWidth);
    Read("viewAltitude", a.viewAltitude);
    Read("sunIntensity", a.sunIntensity);
    Read("multiscatterStrength", a.multiscatterStrength);
    Read("groundAlbedo", a.groundAlbedo);
    Read("marchSteps", a.marchSteps);
  }
  auto ToJson(const Transform &t) -> json {
    return {{"position", t.position}, {"rotation", t.rotation}, {"scale", t.scale}};
  }
  auto FromJson(const json &j, Transform &t) -> void {
    if (j.contains("position"))
      j.at("position").get_to(t.position);
    if (j.contains("rotation"))
      j.at("rotation").get_to(t.rotation);
    if (j.contains("scale"))
      j.at("scale").get_to(t.scale);
  }
  auto ToJson(const BoundingBox &b) -> json {
    return {{"min", b.min}, {"max", b.max}};
  }
  auto FromJson(const json &j, BoundingBox &b) -> void {
    if (j.contains("min"))
      j.at("min").get_to(b.min);
    if (j.contains("max"))
      j.at("max").get_to(b.max);
  }
  auto ToJson(const Camera &c) -> json {
    return {
      {"type", static_cast<int>(c.type)},
      {"position", c.position},
      {"rotation", c.rotation},
      {"fov", c.fov},
      {"nearPlane", c.nearPlane},
      {"farPlane", c.farPlane},
      {"orthoSize", c.orthoSize},
      {"exposureMode", static_cast<int>(c.exposureMode)},
      {"exposure", c.exposure},
      {"aperture", c.aperture},
      {"shutterSpeed", c.shutterSpeed},
      {"sensitivity", c.sensitivity},
    };
  }
  auto FromJson(const json &j, Camera &c) -> void {
    if (j.contains("type"))
      c.type = static_cast<CameraType>(j.at("type").get<int>());
    if (j.contains("fov"))
      j.at("fov").get_to(c.fov);
    if (j.contains("nearPlane"))
      j.at("nearPlane").get_to(c.nearPlane);
    if (j.contains("farPlane"))
      j.at("farPlane").get_to(c.farPlane);
    if (j.contains("orthoSize"))
      j.at("orthoSize").get_to(c.orthoSize);
    // Absent in a scene written before the camera carried an exposure, which is why every one of
    // these is optional: such a scene means the default, and reading it must not disturb what the
    // camera already holds.
    if (j.contains("exposureMode"))
      c.exposureMode = static_cast<ExposureMode>(j.at("exposureMode").get<int>());
    if (j.contains("exposure"))
      j.at("exposure").get_to(c.exposure);
    if (j.contains("aperture"))
      j.at("aperture").get_to(c.aperture);
    if (j.contains("shutterSpeed"))
      j.at("shutterSpeed").get_to(c.shutterSpeed);
    if (j.contains("sensitivity"))
      j.at("sensitivity").get_to(c.sensitivity);
    Transform transform;
    if (j.contains("position"))
      j.at("position").get_to(transform.position);
    if (j.contains("rotation"))
      j.at("rotation").get_to(transform.rotation);
    c.SetTransform(transform);
  }
  /// @brief The rotation a light is saved with: its own, unless that is not the way it is pointing.
  ///
  /// A light keeps a rotation and the direction that follows from it, and loading sets the second
  /// from the first. Code is free to write the direction alone, though, and a game building its
  /// scene does -- `light.forward = ...` -- which leaves a rotation that says something else. Saved
  /// as it stood, such a light came back pointing along -Z: a sun set by a game was on the horizon
  /// after one round trip. So where the two disagree it is the direction that is believed, and the
  /// rotation written is one that gives it.
  auto SavedRotation(const Light &l) -> glm::quat {
    const auto length = glm::length(l.forward);
    if (length < 1e-6f)
      return l.rotation;
    const auto forward = l.forward / length;
    if (glm::dot(l.rotation * glm::vec3(.0f, .0f, -1.f), forward) > .99999f)
      return l.rotation;
    const auto up = std::abs(forward.y) > .999f ? glm::vec3(.0f, .0f, 1.f) : glm::vec3(.0f, 1.f, .0f);
    return glm::quatLookAt(forward, up);
  }
  auto ToJson(const Light &l) -> json {
    return {
      {"type", static_cast<int>(l.type)},
      {"position", l.position},
      {"rotation", SavedRotation(l)},
      {"ambient", l.ambient},
      {"diffuse", l.diffuse},
      {"specular", l.specular},
      {"intensity", l.intensity},
      {"constant", l.constant},
      {"linear", l.linear},
      {"quadratic", l.quadratic},
      {"innerCutoff", l.innerCutoff},
      {"outerCutoff", l.outerCutoff},
      {"nearPlane", l.nearPlane},
      {"farPlane", l.farPlane},
      {"orthoSize", l.orthoSize},
      {"angularDiameter", l.angularDiameter},
      {"sourceRadius", l.sourceRadius},
    };
  }
  auto FromJson(const json &j, Light &l) -> void {
    if (j.contains("type"))
      l.type = static_cast<LightType>(j.at("type").get<int>());
    if (j.contains("ambient"))
      j.at("ambient").get_to(l.ambient);
    if (j.contains("diffuse"))
      j.at("diffuse").get_to(l.diffuse);
    if (j.contains("specular"))
      j.at("specular").get_to(l.specular);
    if (j.contains("intensity"))
      j.at("intensity").get_to(l.intensity);
    if (j.contains("constant"))
      j.at("constant").get_to(l.constant);
    if (j.contains("linear"))
      j.at("linear").get_to(l.linear);
    if (j.contains("quadratic"))
      j.at("quadratic").get_to(l.quadratic);
    if (j.contains("innerCutoff"))
      j.at("innerCutoff").get_to(l.innerCutoff);
    if (j.contains("outerCutoff"))
      j.at("outerCutoff").get_to(l.outerCutoff);
    if (j.contains("nearPlane"))
      j.at("nearPlane").get_to(l.nearPlane);
    if (j.contains("farPlane"))
      j.at("farPlane").get_to(l.farPlane);
    if (j.contains("orthoSize"))
      j.at("orthoSize").get_to(l.orthoSize);
    if (j.contains("angularDiameter"))
      j.at("angularDiameter").get_to(l.angularDiameter);
    if (j.contains("sourceRadius"))
      j.at("sourceRadius").get_to(l.sourceRadius);
    Transform transform;
    if (j.contains("position"))
      j.at("position").get_to(transform.position);
    if (j.contains("rotation"))
      j.at("rotation").get_to(transform.rotation);
    l.SetTransform(transform);
  }
  auto ToJson(const MeshHandle &h) -> json {
    return {{"assetId", h.assetId}};
  }
  auto FromJson(const json &j, MeshHandle &h) -> void {
    if (j.contains("assetId"))
      j.at("assetId").get_to(h.assetId);
  }
  auto ToJson(const MaterialHandle &h) -> json {
    return {{"assetId", h.assetId}};
  }
  auto FromJson(const json &j, MaterialHandle &h) -> void {
    if (j.contains("assetId"))
      j.at("assetId").get_to(h.assetId);
  }
  auto ToJson(const SkyboxHandle &h) -> json {
    return {{"assetId", h.assetId}};
  }
  auto FromJson(const json &j, SkyboxHandle &h) -> void {
    if (j.contains("assetId"))
      j.at("assetId").get_to(h.assetId);
  }
  auto ToJson(const TextureHandle &h) -> json {
    return {{"assetId", h.assetId}};
  }
  auto FromJson(const json &j, TextureHandle &h) -> void {
    if (j.contains("assetId"))
      j.at("assetId").get_to(h.assetId);
  }
  /// @brief Where each entity comes in the file, counting down through the tree as it is written.
  ///
  /// What a component that points at another entity saves in place of its id. An id is only the
  /// number this run happened to give the entity, and loading makes every entity afresh with
  /// numbers of its own; its place in the file is the same on the way out and on the way in.
  using EntityOrder = std::unordered_map<EntityID, int>;
  auto ToJson(const Animator &a, const EntityOrder &order) -> json {
    json j{{"modelAssetId", a.modelAssetId}, {"clipIndex", a.clipIndex}, {"time", a.time}, {"playing", a.playing}, {"loop", a.loop}};
    // What the states are, and not which one is playing or how far in: a scene is saved as it was
    // set up, and one that came back half way through a punch would be a surprise to load.
    if (!a.rootNode.empty())
      j["rootNode"] = a.rootNode;
    if (!a.states.empty()) {
      auto states = json::array();
      for (const auto &state : a.states) {
        auto events = json::array();
        for (const auto &event : state.events)
          events.push_back({{"tick", event.tick}, {"name", event.name}});
        auto blend = json::array();
        for (const auto &entry : state.blend)
          blend.push_back({{"source", entry.source}, {"clip", entry.clip}, {"at", entry.at}});
        states.push_back({{"name", state.name}, {"blend", std::move(blend)}, {"source", state.source}, {"clip", state.clip}, {"loop", state.loop}, {"speed", state.speed}, {"fade", state.fade}, {"next", state.next}, {"events", std::move(events)}});
      }
      j["states"] = std::move(states);
    }
    if (!a.ik.empty()) {
      auto goals = json::array();
      for (const auto &goal : a.ik) {
        const auto target = order.find(goal.target);
        goals.push_back({{"name", goal.name}, {"kind", static_cast<int>(goal.kind)}, {"bone", goal.bone}, {"target", target != order.end() ? target->second : -1}, {"offset", {goal.offset.x, goal.offset.y, goal.offset.z}}, {"weight", goal.weight}, {"bend", {goal.bend.x, goal.bend.y, goal.bend.z}}, {"holdRotation", goal.holdRotation}, {"axis", {goal.axis.x, goal.axis.y, goal.axis.z}}, {"chain", goal.chain}, {"limit", goal.limit}});
      }
      j["ik"] = std::move(goals);
    }
    return j;
  }
  auto FromJson(const json &j, Animator &a) -> void {
    if (j.contains("modelAssetId"))
      j.at("modelAssetId").get_to(a.modelAssetId);
    if (j.contains("clipIndex"))
      j.at("clipIndex").get_to(a.clipIndex);
    if (j.contains("time"))
      j.at("time").get_to(a.time);
    if (j.contains("playing"))
      j.at("playing").get_to(a.playing);
    if (j.contains("loop"))
      j.at("loop").get_to(a.loop);
    if (j.contains("rootNode"))
      j.at("rootNode").get_to(a.rootNode);
    if (j.contains("states") && j.at("states").is_array())
      for (const auto &entry : j.at("states")) {
        AnimationState state;
        state.name = entry.value("name", std::string{});
        state.clip = entry.value("clip", std::string{});
        if (entry.contains("source"))
          entry.at("source").get_to(state.source);
        if (entry.contains("blend") && entry.at("blend").is_array())
          for (const auto &mixed : entry.at("blend")) {
            BlendClip clip;
            if (mixed.contains("source"))
              mixed.at("source").get_to(clip.source);
            clip.clip = mixed.value("clip", std::string{});
            clip.at = mixed.value("at", .0f);
            state.blend.push_back(std::move(clip));
          }
        state.loop = entry.value("loop", false);
        state.speed = entry.value("speed", 1.f);
        state.fade = entry.value("fade", state.fade);
        state.next = entry.value("next", std::string{});
        if (entry.contains("events") && entry.at("events").is_array())
          for (const auto &event : entry.at("events"))
            state.events.push_back({event.value("tick", 0), event.value("name", std::string{})});
        a.states.push_back(std::move(state));
      }
    // The target is read as the place in the file it was saved as, and left that way until every
    // entity has been made and there is an id to give it. See `ResolveTargets`.
    if (j.contains("ik") && j.at("ik").is_array())
      for (const auto &entry : j.at("ik")) {
        IKGoal goal;
        const auto Vector = [&](const char *key, glm::vec3 &value) {
          if (entry.contains(key) && entry.at(key).is_array() && entry.at(key).size() == 3)
            value = {entry.at(key)[0].get<float>(), entry.at(key)[1].get<float>(), entry.at(key)[2].get<float>()};
        };
        goal.name = entry.value("name", std::string{});
        goal.kind = entry.value("kind", 0) == static_cast<int>(IKKind::Aim) ? IKKind::Aim : IKKind::Reach;
        goal.bone = entry.value("bone", std::string{});
        goal.target = EntityID{entry.value("target", -1LL)};
        Vector("offset", goal.offset);
        goal.weight = entry.value("weight", goal.weight);
        Vector("bend", goal.bend);
        goal.holdRotation = entry.value("holdRotation", goal.holdRotation);
        Vector("axis", goal.axis);
        goal.chain = entry.value("chain", goal.chain);
        goal.limit = entry.value("limit", goal.limit);
        a.ik.push_back(std::move(goal));
      }
  }
  auto ToJson(const ModelMeshHandle &h) -> json {
    return {{"modelAssetId", h.modelAssetId}, {"meshIndex", h.meshIndex}};
  }
  auto FromJson(const json &j, ModelMeshHandle &h) -> void {
    if (j.contains("modelAssetId"))
      j.at("modelAssetId").get_to(h.modelAssetId);
    if (j.contains("meshIndex"))
      j.at("meshIndex").get_to(h.meshIndex);
  }
  auto ToJson(const ModelMaterialHandle &h) -> json {
    return {{"modelAssetId", h.modelAssetId}, {"materialIndex", h.materialIndex}, {"textureIndices", h.textureIndices}};
  }
  auto FromJson(const json &j, ModelMaterialHandle &h) -> void {
    if (j.contains("modelAssetId"))
      j.at("modelAssetId").get_to(h.modelAssetId);
    if (j.contains("materialIndex"))
      j.at("materialIndex").get_to(h.materialIndex);
    if (j.contains("textureIndices"))
      j.at("textureIndices").get_to(h.textureIndices);
  }
  auto ToJson(const VolumetricClouds &c) -> json {
    return {
      {"bottomAltitude", c.bottomAltitude},
      {"topAltitude", c.topAltitude},
      {"coverage", c.coverage},
      {"density", c.density},
      {"windDirection", c.windDirection},
      {"windSpeed", c.windSpeed},
      {"shapeScale", c.shapeScale},
      {"detailScale", c.detailScale},
      {"detailStrength", c.detailStrength},
      {"anisotropy", c.anisotropy},
      {"backscatter", c.backscatter},
      {"powder", c.powder},
      {"ambient", c.ambient},
      {"scatteringScale", c.scatteringScale},
      {"shadowStrength", c.shadowStrength},
      {"steps", c.steps},
      {"lightSteps", c.lightSteps}};
  }
  auto FromJson(const json &j, VolumetricClouds &c) -> void {
    const auto Read = [&j](const char *key, auto &field) {
      if (j.contains(key))
        j.at(key).get_to(field);
    };
    Read("bottomAltitude", c.bottomAltitude);
    Read("topAltitude", c.topAltitude);
    Read("coverage", c.coverage);
    Read("density", c.density);
    Read("windDirection", c.windDirection);
    Read("windSpeed", c.windSpeed);
    Read("shapeScale", c.shapeScale);
    Read("detailScale", c.detailScale);
    Read("detailStrength", c.detailStrength);
    Read("anisotropy", c.anisotropy);
    Read("backscatter", c.backscatter);
    Read("powder", c.powder);
    Read("ambient", c.ambient);
    Read("scatteringScale", c.scatteringScale);
    Read("shadowStrength", c.shadowStrength);
    Read("steps", c.steps);
    Read("lightSteps", c.lightSteps);
  }
  auto ToJson(const VolumetricFog &f) -> json {
    return {
      {"density", f.density},
      {"baseHeight", f.baseHeight},
      {"heightFalloff", f.heightFalloff},
      {"albedo", f.albedo},
      {"anisotropy", f.anisotropy},
      {"ambient", f.ambient},
      {"lightScale", f.lightScale},
      {"maxDistance", f.maxDistance},
      {"aerialPerspective", f.aerialPerspective},
      {"kilometresPerUnit", f.kilometresPerUnit},
      {"steps", f.steps}};
  }
  auto FromJson(const json &j, VolumetricFog &f) -> void {
    const auto Read = [&j](const char *key, auto &field) {
      if (j.contains(key))
        j.at(key).get_to(field);
    };
    Read("density", f.density);
    Read("baseHeight", f.baseHeight);
    Read("heightFalloff", f.heightFalloff);
    Read("albedo", f.albedo);
    Read("anisotropy", f.anisotropy);
    Read("ambient", f.ambient);
    Read("lightScale", f.lightScale);
    Read("maxDistance", f.maxDistance);
    Read("aerialPerspective", f.aerialPerspective);
    Read("kilometresPerUnit", f.kilometresPerUnit);
    Read("steps", f.steps);
  }
  auto ToJson(const Stylization &s) -> json {
    return {
      {"bands", s.bands},
      {"bandSoftness", s.bandSoftness},
      {"terminator", s.terminator},
      {"shadowTint", s.shadowTint},
      {"highlightSize", s.highlightSize},
      {"rimIntensity", s.rimIntensity},
      {"rimWidth", s.rimWidth},
      {"inkThickness", s.inkThickness},
      {"inkDepthThreshold", s.inkDepthThreshold},
      {"inkCreaseThreshold", s.inkCreaseThreshold},
      {"inkColor", s.inkColor},
      {"inkTint", s.inkTint},
      {"inkFadeDistance", s.inkFadeDistance},
      {"saturation", s.saturation},
      {"contrast", s.contrast},
      {"shadowColor", s.shadowColor},
      {"highlightColor", s.highlightColor},
      {"vignette", s.vignette}};
  }
  auto FromJson(const json &j, Stylization &s) -> void {
    const auto Read = [&j](const char *key, auto &field) {
      if (j.contains(key))
        j.at(key).get_to(field);
    };
    Read("bands", s.bands);
    Read("bandSoftness", s.bandSoftness);
    Read("terminator", s.terminator);
    Read("shadowTint", s.shadowTint);
    Read("highlightSize", s.highlightSize);
    Read("rimIntensity", s.rimIntensity);
    Read("rimWidth", s.rimWidth);
    Read("inkThickness", s.inkThickness);
    Read("inkDepthThreshold", s.inkDepthThreshold);
    Read("inkCreaseThreshold", s.inkCreaseThreshold);
    Read("inkColor", s.inkColor);
    Read("inkTint", s.inkTint);
    Read("inkFadeDistance", s.inkFadeDistance);
    Read("saturation", s.saturation);
    Read("contrast", s.contrast);
    Read("shadowColor", s.shadowColor);
    Read("highlightColor", s.highlightColor);
    Read("vignette", s.vignette);
  }
  auto ToJson(const ParticleEmitter &p) -> json {
    return {
      {"capacity", p.capacity},
      {"rate", p.rate},
      {"emitting", p.emitting},
      {"burst", p.burst},
      {"shape", static_cast<int>(p.shape)},
      {"mesh", p.mesh},
      {"lifetime", p.lifetime},
      {"lifetimeVariance", p.lifetimeVariance},
      {"hold", p.hold},
      {"holdVariance", p.holdVariance},
      {"speed", p.speed},
      {"speedVariance", p.speedVariance},
      {"spread", p.spread},
      {"radius", p.radius},
      {"gravity", p.gravity},
      {"drag", p.drag},
      {"startSize", p.startSize},
      {"endSize", p.endSize},
      {"startColor", p.startColor},
      {"endColor", p.endColor},
      {"blend", static_cast<int>(p.blend)},
      {"intensity", p.intensity}};
  }
  auto FromJson(const json &j, ParticleEmitter &p) -> void {
    const auto Read = [&j](const char *key, auto &field) {
      if (j.contains(key))
        j.at(key).get_to(field);
    };
    Read("capacity", p.capacity);
    Read("rate", p.rate);
    Read("emitting", p.emitting);
    // `fired` is left out on both sides. It counts the bursts asked for in a run, and a scene that
    // saved it would be saving how many times somebody pressed a button.
    Read("burst", p.burst);
    if (j.contains("shape"))
      p.shape = static_cast<ParticleShape>(j.at("shape").get<int>());
    Read("mesh", p.mesh);
    Read("lifetime", p.lifetime);
    Read("lifetimeVariance", p.lifetimeVariance);
    Read("hold", p.hold);
    Read("holdVariance", p.holdVariance);
    Read("speed", p.speed);
    Read("speedVariance", p.speedVariance);
    Read("spread", p.spread);
    Read("radius", p.radius);
    Read("gravity", p.gravity);
    Read("drag", p.drag);
    Read("startSize", p.startSize);
    Read("endSize", p.endSize);
    Read("startColor", p.startColor);
    Read("endColor", p.endColor);
    if (j.contains("blend"))
      p.blend = static_cast<ParticleBlend>(j.at("blend").get<int>());
    Read("intensity", p.intensity);
  }
  struct ToJsonVisitor {
    std::unordered_set<AssetID> &referenced;
    const EntityOrder &order;
    auto operator()(Transform *t) const -> json {
      return ToJson(*t);
    }
    auto operator()(AtmosphereSky *a) const -> json {
      return ToJson(*a);
    }
    auto operator()(VolumetricClouds *c) const -> json {
      return ToJson(*c);
    }
    auto operator()(VolumetricFog *f) const -> json {
      return ToJson(*f);
    }
    auto operator()(Stylization *s) const -> json {
      return ToJson(*s);
    }
    auto operator()(ParticleEmitter *p) const -> json {
      if (p->mesh)
        referenced.insert(p->mesh);
      return ToJson(*p);
    }
    auto operator()(BoundingBox *b) const -> json {
      return ToJson(*b);
    }
    auto operator()(Camera *c) const -> json {
      return ToJson(*c);
    }
    auto operator()(Light *l) const -> json {
      return ToJson(*l);
    }
    auto operator()(Animator *a) const -> json {
      if (a->modelAssetId)
        referenced.insert(a->modelAssetId);
      for (const auto &state : a->states) {
        if (state.source)
          referenced.insert(state.source);
        for (const auto &entry : state.blend)
          if (entry.source)
            referenced.insert(entry.source);
      }
      return ToJson(*a, order);
    }
    auto operator()(MeshHandle *h) const -> json {
      if (h->assetId)
        referenced.insert(h->assetId);
      return ToJson(*h);
    }
    auto operator()(MaterialHandle *h) const -> json {
      if (h->assetId)
        referenced.insert(h->assetId);
      return ToJson(*h);
    }
    auto operator()(SkyboxHandle *h) const -> json {
      if (h->assetId)
        referenced.insert(h->assetId);
      return ToJson(*h);
    }
    auto operator()(TextureHandle *h) const -> json {
      if (h->assetId)
        referenced.insert(h->assetId);
      return ToJson(*h);
    }
    auto operator()(ModelMeshHandle *h) const -> json {
      if (h->modelAssetId)
        referenced.insert(h->modelAssetId);
      return ToJson(*h);
    }
    auto operator()(ModelMaterialHandle *h) const -> json {
      if (h->modelAssetId)
        referenced.insert(h->modelAssetId);
      return ToJson(*h);
    }
    template <typename T>
    auto operator()(T) const -> json {
      return {};
    }
  };
  auto SerializeEntity(Application &app, const EntityID id, std::unordered_set<AssetID> &referenced, const EntityOrder &order, const ComponentNames &names) -> json {
    json entity;
    const auto name = app.GetEntityName(id);
    entity["name"] = name;
    // Every component of the entity's archetype by name, the ones that are not written below as well
    // as the ones that are: the archetype is fixed when an entity is made, so the file has to say
    // which one to make it of, and the components it has values for are not enough to tell.
    auto archetype = json::array();
    const auto signature = app.GetEntitySignature(id);
    for (size_t bit = 0; bit < MAX_COMPONENTS; ++bit)
      if (signature[bit])
        if (auto it = names.byId.find(static_cast<ComponentId>(bit)); it != names.byId.end())
          archetype.push_back(it->second);
    entity["archetype"] = std::move(archetype);
    auto components = json::object();
    for (const auto type : app.GetEntityComponentTypes(id)) {
      if (!IsSerialized(type))
        continue;
      if (auto variant = app.GetEntityComponent(id, type); variant.has_value())
        components[Component::GetTypeName(type)] = std::visit(ToJsonVisitor{referenced, order}, *variant);
    }
    entity["components"] = std::move(components);
    auto children = json::array();
    app.ForEachChildEntity(id, [&](const EntityID childId) {
      children.push_back(SerializeEntity(app, childId, referenced, order, names));
    });
    entity["children"] = std::move(children);
    return entity;
  }
  /// @brief Numbers every entity in the order `SerializeEntity` will write them: each one, then its children.
  auto Order(Application &app, const EntityID id, EntityOrder &order) -> void {
    order.emplace(id, static_cast<int>(order.size()));
    app.ForEachChildEntity(id, [&](const EntityID childId) {
      Order(app, childId, order);
    });
  }
  /// @brief Swaps the places in the file the goals were loaded with for the entities made from them.
  auto ResolveTargets(Application &app, const std::vector<EntityID> &created) -> void {
    app.ForEachEntity<Animator>([&](const EntityID, Animator *animator) {
      for (auto &goal : animator->ik) {
        const auto place = static_cast<long long>(goal.target);
        goal.target = place >= 0 && place < static_cast<long long>(created.size()) ? created[place] : EntityID::Invalid;
      }
    });
  }
  auto DeserializeEntity(Application &app, const json &entityJson, const EntityID parent, const std::unordered_map<AssetID, AssetID> &remap, std::vector<EntityID> &created, const ComponentNames &names) -> void {
    const auto name = entityJson.value("name", std::string{});
    // The archetype the file names, or failing that the one its components suggest. Either way it
    // must be one the application declared; an entity of any other is reported and left out, with
    // its children brought in at the top of the scene.
    ComponentMask mask;
    auto known = true;
    if (entityJson.contains("archetype")) {
      for (const auto &componentName : entityJson.at("archetype")) {
        if (auto it = names.byName.find(componentName.get<std::string>()); it != names.byName.end())
          mask.set(it->second);
        else
          known = false;
      }
    } else if (entityJson.contains("components")) {
      ComponentMask saved;
      const auto &nameToType = NameToType();
      for (const auto &[typeName, componentJson] : entityJson.at("components").items())
        if (auto it = nameToType.find(typeName); it != nameToType.end())
          saved.set(static_cast<size_t>(it->second));
      if (auto guessed = GuessArchetype(app, saved); guessed)
        mask = *guessed;
      else
        known = false;
    }
    if (entityJson.contains("scripts"))
      spdlog::warn("[SceneSerializer] Entity '{}' names scripts, which are no longer a thing an entity can have; its behaviour belongs to a system now", name);
    const auto id = known ? app.CreateEntity(mask, name) : EntityID::Invalid;
    if (!id)
      spdlog::error("[SceneSerializer] Entity '{}' is of an archetype this application does not declare, and was left out", name);
    created.push_back(id);
    if (id && parent)
      app.AddChildEntity(parent, id);
    if (id && entityJson.contains("components"))
      for (const auto &[typeName, componentJson] : entityJson.at("components").items()) {
        const auto &nameToType = NameToType();
        const auto it = nameToType.find(typeName);
        if (it == nameToType.end())
          continue;
        switch (it->second) {
        case ComponentType::Transform:
          if (auto *c = app.GetEntityComponent<Transform>(id))
            FromJson(componentJson, *c);
          break;
        case ComponentType::AtmosphereSky:
          if (auto *c = app.GetEntityComponent<AtmosphereSky>(id))
            FromJson(componentJson, *c);
          break;
        case ComponentType::VolumetricClouds:
          if (auto *c = app.GetEntityComponent<VolumetricClouds>(id))
            FromJson(componentJson, *c);
          break;
        case ComponentType::VolumetricFog:
          if (auto *c = app.GetEntityComponent<VolumetricFog>(id))
            FromJson(componentJson, *c);
          break;
        case ComponentType::Stylization:
          if (auto *c = app.GetEntityComponent<Stylization>(id))
            FromJson(componentJson, *c);
          break;
        case ComponentType::ParticleEmitter: {
          auto p = app.GetEntityComponent<ParticleEmitter>(id);
          if (!p)
            break;
          FromJson(componentJson, *p);
          p->mesh = Remap(remap, p->mesh);
          break;
        }
        case ComponentType::BoundingBox:
          if (auto *c = app.GetEntityComponent<BoundingBox>(id))
            FromJson(componentJson, *c);
          break;
        case ComponentType::Camera:
          if (auto *c = app.GetEntityComponent<Camera>(id))
            FromJson(componentJson, *c);
          break;
        case ComponentType::Light:
          if (auto *c = app.GetEntityComponent<Light>(id))
            FromJson(componentJson, *c);
          break;
        case ComponentType::Animator: {
          auto a = app.GetEntityComponent<Animator>(id);
          if (!a)
            break;
          FromJson(componentJson, *a);
          a->modelAssetId = Remap(remap, a->modelAssetId);
          for (auto &state : a->states) {
            if (state.source)
              state.source = Remap(remap, state.source);
            for (auto &entry : state.blend)
              if (entry.source)
                entry.source = Remap(remap, entry.source);
          }
          break;
        }
        case ComponentType::MeshHandle: {
          auto h = app.GetEntityComponent<MeshHandle>(id);
          if (!h)
            break;
          FromJson(componentJson, *h);
          h->assetId = Remap(remap, h->assetId);
          break;
        }
        case ComponentType::MaterialHandle: {
          auto h = app.GetEntityComponent<MaterialHandle>(id);
          if (!h)
            break;
          FromJson(componentJson, *h);
          h->assetId = Remap(remap, h->assetId);
          break;
        }
        case ComponentType::SkyboxHandle: {
          auto h = app.GetEntityComponent<SkyboxHandle>(id);
          if (!h)
            break;
          FromJson(componentJson, *h);
          h->assetId = Remap(remap, h->assetId);
          break;
        }
        case ComponentType::TextureHandle: {
          auto h = app.GetEntityComponent<TextureHandle>(id);
          if (!h)
            break;
          FromJson(componentJson, *h);
          h->assetId = Remap(remap, h->assetId);
          break;
        }
        case ComponentType::ModelMeshHandle: {
          auto h = app.GetEntityComponent<ModelMeshHandle>(id);
          if (!h)
            break;
          FromJson(componentJson, *h);
          h->modelAssetId = Remap(remap, h->modelAssetId);
          break;
        }
        case ComponentType::ModelMaterialHandle: {
          auto h = app.GetEntityComponent<ModelMaterialHandle>(id);
          if (!h)
            break;
          FromJson(componentJson, *h);
          h->modelAssetId = Remap(remap, h->modelAssetId);
          break;
        }
        default:
          break;
        }
      }
    if (entityJson.contains("children"))
      for (const auto &childJson : entityJson.at("children"))
        DeserializeEntity(app, childJson, id, remap, created, names);
  }
} // namespace
auto SceneSerializer::Save(Application &app, const std::filesystem::path &path) -> bool {
  if (!app.GetScene()) {
    spdlog::error("[SceneSerializer] No active scene to save");
    return false;
  }
  const auto scenePath = ResolvePath(path);
  std::unordered_set<AssetID> referenced;
  auto entities = json::array();
  // The pose, and not the pose with the goals solved on it. A bone a clip moves is written over
  // on every frame and would forget the goals' turn anyway; one no clip moves would keep it, and
  // the next goal would start from there -- a hand reached out once, saved, and reaching from
  // wherever it was left ever after. Solved again on the next frame, before anything is drawn.
  app.ForEachEntity<Animator>([&](const EntityID, Animator *animator) {
    UndoInverseKinematics(app, *animator);
  });
  EntityOrder order;
  app.ForEachRootEntity([&](const EntityID id) {
    Order(app, id, order);
  });
  const auto names = GetComponentNames(app);
  app.ForEachRootEntity([&](const EntityID id) {
    entities.push_back(SerializeEntity(app, id, referenced, order, names));
  });
  json sceneJson;
  sceneJson["name"] = scenePath.stem().string();
  sceneJson["entities"] = std::move(entities);
  std::ofstream sceneFile(scenePath);
  if (!sceneFile) {
    spdlog::error("[SceneSerializer] Failed to open file for writing: {}", scenePath.string());
    return false;
  }
  sceneFile << sceneJson.dump(2);
  sceneFile.close();
  auto manifest = json::array();
  for (const auto &assetId : referenced) {
    json entry;
    entry["id"] = assetId;
    entry["name"] = app.GetAssetName(assetId);
    entry["type"] = static_cast<int>(app.GetAssetType(assetId));
    const auto path = app.GetAssetPath(assetId);
    entry["path"] = path.empty() ? std::string{} : path.lexically_normal().string();
    manifest.push_back(std::move(entry));
  }
  const auto manifestPath = ManifestPath(scenePath);
  std::ofstream manifestFile(manifestPath);
  if (!manifestFile) {
    spdlog::error("[SceneSerializer] Failed to open file for writing: {}", manifestPath.string());
    return false;
  }
  manifestFile << manifest.dump(2);
  manifestFile.close();
  spdlog::info("[SceneSerializer] Saved scene: {} ({} referenced assets)", scenePath.string(), referenced.size());
  return true;
}
auto SceneSerializer::Load(Application &app, const std::filesystem::path &path) -> bool {
  const auto scenePath = ResolvePath(path);
  std::ifstream sceneFile(scenePath);
  if (!sceneFile) {
    spdlog::error("[SceneSerializer] Failed to open file for reading: {}", scenePath.string());
    return false;
  }
  json sceneJson;
  try {
    sceneFile >> sceneJson;
  } catch (const json::parse_error &e) {
    spdlog::error("[SceneSerializer] Failed to parse scene file: {} ({})", scenePath.string(), e.what());
    return false;
  }
  std::unordered_map<AssetID, AssetID> remap;
  const auto manifestPath = ManifestPath(scenePath);
  if (std::ifstream manifestFile(manifestPath); manifestFile) {
    json manifest;
    try {
      manifestFile >> manifest;
      for (const auto &entry : manifest) {
        const auto savedId = entry.at("id").get<AssetID>();
        const auto name = entry.value("name", std::string{});
        const auto pathStr = entry.value("path", std::string{});
        auto runtimeId = AssetID{};
        if (!pathStr.empty()) {
          switch (static_cast<AssetType>(entry.value("type", 0))) {
          case AssetType::Material:
            runtimeId = app.LoadAssetAsync<MaterialAsset>(pathStr, name, savedId);
            break;
          case AssetType::Model:
            runtimeId = app.LoadAssetAsync<ModelAsset>(pathStr, name, savedId);
            break;
          case AssetType::Shader:
            runtimeId = app.LoadAssetAsync<ShaderAsset>(pathStr, name, savedId);
            break;
          case AssetType::Texture:
            runtimeId = app.LoadAssetAsync<TextureAsset>(pathStr, name, savedId);
            break;
          default:
            break;
          }
        }
        if (runtimeId)
          remap.emplace(savedId, runtimeId);
        else if (!app.GetAsset(savedId))
          spdlog::warn("[SceneSerializer] Failed to resolve referenced asset: {}", name);
      }
    } catch (const json::parse_error &e) {
      spdlog::warn("[SceneSerializer] Failed to parse asset manifest: {} ({})", manifestPath.string(), e.what());
    }
  } else
    spdlog::warn("[SceneSerializer] No asset manifest found: {}", manifestPath.string());
  app.DeleteEntities();
  std::vector<EntityID> created;
  const auto names = GetComponentNames(app);
  if (sceneJson.contains("entities"))
    for (const auto &entityJson : sceneJson.at("entities"))
      DeserializeEntity(app, entityJson, EntityID::Invalid, remap, created, names);
  ResolveTargets(app, created);
  // Gathered first and resolved after, not from inside the walk over the animators. Resolving used
  // to add a `Skeleton`, and a component added while a `ForEach` was under way was put off until it
  // ended -- so the skeleton arrived empty and every model in a loaded scene stood still. It only
  // writes into the skeleton the entity was made with now, but the walk is no place to be calling
  // back into the asset manager from either way.
  std::vector<std::pair<EntityID, AssetID>> instances;
  app.ForEachEntity<Animator>([&](const EntityID id, Animator *animator) {
    instances.emplace_back(id, animator->modelAssetId);
  });
  for (const auto &[id, model] : instances)
    app.ResolveModelInstance(id, model);
  spdlog::info("[SceneSerializer] Loaded scene: {}", scenePath.string());
  return true;
}
} // namespace kuki
