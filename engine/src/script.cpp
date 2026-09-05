#include <application.hpp>
#include <script.hpp>
#include <string_view>
#include <utility>
namespace kuki {
auto Script::GetApp() const -> Application * {
  return app;
}
auto Script::MarkDirty() -> void {
  if (app && entityId)
    app->MarkTransformDirty(entityId);
}
auto Script::IsAttached() const -> bool {
  return app && entityId && app->IsEntity(entityId);
}
auto Script::GetEntityName() const -> std::string {
  // Not `GetTypeName`, which answers a different question: that one is the name of the script
  // type, this one the name of the entity carrying it.
  if (!app || !entityId)
    return {};
  return app->GetEntityName(entityId);
}
auto Script::GetParent() const -> EntityID {
  if (!app || !entityId)
    return EntityID::Invalid;
  return app->GetEntityParent(entityId);
}
auto Script::HasChildren() const -> bool {
  return app && entityId && app->EntityHasChildren(entityId);
}
auto Script::AddChild(const EntityID child) -> bool {
  if (!app || !entityId)
    return false;
  return app->AddChildEntity(entityId, child);
}
auto Script::Rename(std::string name) -> bool {
  if (!app || !entityId)
    return false;
  return app->RenameEntity(entityId, std::move(name));
}
auto Script::SetActiveCamera() -> bool {
  if (!app || !entityId)
    return false;
  return app->SetActiveCamera(entityId);
}
auto Script::AlignView() -> void {
  if (app && entityId)
    app->AlignView(entityId);
}
auto Script::GetComponentTypes() const -> std::vector<ComponentType> {
  if (!app || !entityId)
    return {};
  return app->GetEntityComponentTypes(entityId);
}
auto Script::ResolveModel(const AssetID modelAssetId) -> void {
  if (app && entityId)
    app->ResolveModelInstance(entityId, modelAssetId);
}
auto Script::Display() const -> void {}
auto Script::GetTypeName() const -> std::string {
  std::string name = typeIndex.name();
  for (const std::string_view prefix : {"class ", "struct "})
    if (name.starts_with(prefix)) {
      name.erase(0, prefix.size());
      break;
    }
  return name;
}
auto Script::GetTypeIndex() const -> std::type_index {
  return typeIndex;
}
auto Script::Start(Application &) -> void {}
auto Script::Update(Application &) -> void {}
auto Script::Shutdown(Application &) -> void {}
} // namespace kuki
