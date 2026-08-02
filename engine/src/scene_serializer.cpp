#include <animator.hpp>
#include <application.hpp>
#include <bounding_box.hpp>
#include <camera.hpp>
#include <component.hpp>
#include <fstream>
#include <gl_skybox.hpp>
#include <light.hpp>
#include <material_handle.hpp>
#include <mesh_handle.hpp>
#include <model_asset.hpp>
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
using json = nlohmann::json;
namespace kuki {
namespace {
  constexpr ComponentType SERIALIZED_TYPES[]{
    ComponentType::Animator,
    ComponentType::BoundingBox,
    ComponentType::Camera,
    ComponentType::Light,
    ComponentType::MaterialHandle,
    ComponentType::MeshHandle,
    ComponentType::ModelMaterialHandle,
    ComponentType::ModelMeshHandle,
    ComponentType::SkyboxHandle,
    ComponentType::TextureHandle,
    ComponentType::Transform,
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
    Transform transform;
    if (j.contains("position"))
      j.at("position").get_to(transform.position);
    if (j.contains("rotation"))
      j.at("rotation").get_to(transform.rotation);
    c.SetTransform(transform);
  }
  auto ToJson(const Light &l) -> json {
    return {
      {"type", static_cast<int>(l.type)},
      {"position", l.position},
      {"rotation", l.rotation},
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
  auto ToJson(const Animator &a) -> json {
    return {{"modelAssetId", a.modelAssetId}, {"clipIndex", a.clipIndex}, {"time", a.time}, {"playing", a.playing}, {"loop", a.loop}};
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
  struct ToJsonVisitor {
    std::unordered_set<AssetID> &referenced;
    auto operator()(Transform *t) const -> json {
      return ToJson(*t);
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
      return ToJson(*a);
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
  auto SerializeEntity(Application &app, const EntityID id, std::unordered_set<AssetID> &referenced) -> json {
    json entity;
    const auto name = app.GetEntityName(id);
    entity["name"] = name;
    auto components = json::object();
    auto hasScript = false;
    for (const auto type : app.GetEntityComponentTypes(id)) {
      if (type == ComponentType::Script) {
        hasScript = true;
        continue;
      }
      if (!IsSerialized(type))
        continue;
      if (auto variant = app.GetEntityComponent(id, type); variant.has_value())
        components[Component::GetTypeName(type)] = std::visit(ToJsonVisitor{referenced}, *variant);
    }
    if (hasScript)
      spdlog::warn("[SceneSerializer] entity '{}' has script components that will not be saved", name);
    entity["components"] = std::move(components);
    auto children = json::array();
    app.ForEachChildEntity(id, [&](const EntityID childId) {
      children.push_back(SerializeEntity(app, childId, referenced));
    });
    entity["children"] = std::move(children);
    return entity;
  }
  auto DeserializeEntity(Application &app, const json &entityJson, const EntityID parent, const std::unordered_map<AssetID, AssetID> &remap) -> void {
    const auto id = app.CreateEntity(entityJson.value("name", std::string{}));
    if (parent)
      app.AddChildEntity(parent, id);
    if (entityJson.contains("components"))
      for (const auto &[typeName, componentJson] : entityJson.at("components").items()) {
        const auto &nameToType = NameToType();
        const auto it = nameToType.find(typeName);
        if (it == nameToType.end())
          continue;
        switch (it->second) {
        case ComponentType::Transform:
          FromJson(componentJson, *app.AddEntityComponent<Transform>(id));
          break;
        case ComponentType::BoundingBox:
          FromJson(componentJson, *app.AddEntityComponent<BoundingBox>(id));
          break;
        case ComponentType::Camera:
          FromJson(componentJson, *app.AddEntityComponent<Camera>(id));
          break;
        case ComponentType::Light:
          FromJson(componentJson, *app.AddEntityComponent<Light>(id));
          break;
        case ComponentType::Animator: {
          auto a = app.AddEntityComponent<Animator>(id);
          FromJson(componentJson, *a);
          a->modelAssetId = Remap(remap, a->modelAssetId);
          break;
        }
        case ComponentType::MeshHandle: {
          auto h = app.AddEntityComponent<MeshHandle>(id);
          FromJson(componentJson, *h);
          h->assetId = Remap(remap, h->assetId);
          break;
        }
        case ComponentType::MaterialHandle: {
          auto h = app.AddEntityComponent<MaterialHandle>(id);
          FromJson(componentJson, *h);
          h->assetId = Remap(remap, h->assetId);
          break;
        }
        case ComponentType::SkyboxHandle: {
          auto h = app.AddEntityComponent<SkyboxHandle>(id);
          FromJson(componentJson, *h);
          h->assetId = Remap(remap, h->assetId);
          app.AddEntityComponent<GLSkybox>(id);
          break;
        }
        case ComponentType::TextureHandle: {
          auto h = app.AddEntityComponent<TextureHandle>(id);
          FromJson(componentJson, *h);
          h->assetId = Remap(remap, h->assetId);
          break;
        }
        case ComponentType::ModelMeshHandle: {
          auto h = app.AddEntityComponent<ModelMeshHandle>(id);
          FromJson(componentJson, *h);
          h->modelAssetId = Remap(remap, h->modelAssetId);
          break;
        }
        case ComponentType::ModelMaterialHandle: {
          auto h = app.AddEntityComponent<ModelMaterialHandle>(id);
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
        DeserializeEntity(app, childJson, id, remap);
  }
} // namespace
auto SceneSerializer::Save(Application &app, const std::filesystem::path &scenePath) -> bool {
  if (!app.GetScene()) {
    spdlog::error("[SceneSerializer] no active scene to save");
    return false;
  }
  std::unordered_set<AssetID> referenced;
  auto entities = json::array();
  app.ForEachRootEntity([&](const EntityID id) {
    entities.push_back(SerializeEntity(app, id, referenced));
  });
  json sceneJson;
  sceneJson["name"] = scenePath.stem().string();
  sceneJson["entities"] = std::move(entities);
  std::ofstream sceneFile(scenePath);
  if (!sceneFile) {
    spdlog::error("[SceneSerializer] failed to open file for writing: {}", scenePath.string());
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
    spdlog::error("[SceneSerializer] failed to open file for writing: {}", manifestPath.string());
    return false;
  }
  manifestFile << manifest.dump(2);
  manifestFile.close();
  spdlog::info("[SceneSerializer] saved scene: {} ({} referenced assets)", scenePath.string(), referenced.size());
  return true;
}
auto SceneSerializer::Load(Application &app, const std::filesystem::path &scenePath) -> bool {
  std::ifstream sceneFile(scenePath);
  if (!sceneFile) {
    spdlog::error("[SceneSerializer] failed to open file for reading: {}", scenePath.string());
    return false;
  }
  json sceneJson;
  try {
    sceneFile >> sceneJson;
  } catch (const json::parse_error &e) {
    spdlog::error("[SceneSerializer] failed to parse scene file: {} ({})", scenePath.string(), e.what());
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
        if (!runtimeId && !name.empty())
          if (auto asset = app.GetAsset(name); asset)
            runtimeId = asset->id;
        if (runtimeId)
          remap.emplace(savedId, runtimeId);
        else
          spdlog::warn("[SceneSerializer] failed to resolve referenced asset: {}", name);
      }
    } catch (const json::parse_error &e) {
      spdlog::warn("[SceneSerializer] failed to parse asset manifest: {} ({})", manifestPath.string(), e.what());
    }
  } else
    spdlog::warn("[SceneSerializer] no asset manifest found: {}", manifestPath.string());
  app.DeleteEntities();
  if (sceneJson.contains("entities"))
    for (const auto &entityJson : sceneJson.at("entities"))
      DeserializeEntity(app, entityJson, EntityID::Invalid, remap);
  app.ForEachEntity<Animator>([&](const EntityID id, Animator *animator) {
    app.ResolveModelInstance(id, animator->modelAssetId);
  });
  spdlog::info("[SceneSerializer] loaded scene: {}", scenePath.string());
  return true;
}
} // namespace kuki
