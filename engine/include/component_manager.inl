template <>
inline auto ComponentManager<Transform>::Sort() -> void {
  const auto count = ActiveCount();
  if (count == 0)
    return;
  std::vector<Transform> components_;
  std::unordered_map<EntityID, size_t> entityToComponent_;
  std::vector<EntityID> componentToEntity_;
  components_.reserve(count);
  entityToComponent_.reserve(count);
  componentToEntity_.reserve(count);
  std::stack<size_t> parents;
  for (auto i = 0; i < count; ++i) {
    auto entityId = componentToEntity[i];
    if (entityToComponent_.find(entityId) != entityToComponent_.end())
      // skip if entity has been processed
      continue;
    auto parentId = components[i].parent;
    while (parentId) {
      if (entityToComponent_.find(parentId) != entityToComponent_.end())
        // skip if parent has been processed
        break;
      if (auto it = entityToComponent.find(parentId); it != entityToComponent.end()) {
        parents.push(it->second);
        parentId = components[it->second].parent;
      } else // TODO: if parent ID is valid, then this is unexpected — throw an exception maybe
        break;
    }
    while (!parents.empty()) {
      const auto componentId = parents.top();
      const auto entityId = componentToEntity[componentId];
      componentToEntity_.push_back(entityId);
      const auto componentId_ = components_.size();
      entityToComponent_.insert({entityId, componentId_});
      auto &component = components[componentId];
      components_.push_back(component);
      parents.pop();
    }
    componentToEntity_.push_back(entityId);
    const auto componentId_ = components_.size();
    entityToComponent_.insert({entityId, componentId_});
    auto &component = components[i];
    components_.push_back(component);
  }
  components = std::move(components_);
  entityToComponent = std::move(entityToComponent_);
  componentToEntity = std::move(componentToEntity_);
  inactiveCount = 0;
}
template <>
inline auto ComponentManager<Transform>::Update() -> void {
  const auto count = ActiveCount();
  if (count == 0)
    return;
  for (auto i = 0; i < count; ++i) {
    auto &transform = components[i];
    const auto &parentId = transform.parent;
    Transform *parentTransform = nullptr;
    if (auto it = entityToComponent.find(parentId); it != entityToComponent.end())
      parentTransform = &components[it->second];
    transform.dirty |= parentTransform && parentTransform->dirty;
    if (transform.dirty)
      transform.Update(parentTransform);
  }
  for (auto &c : components)
    c.dirty = false;
}
