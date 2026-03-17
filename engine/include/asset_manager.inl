template <>
inline auto AssetManager::CreatePrefab<SceneAsset>(const AssetID assetId) -> EntityID {
  if (auto sceneAsset = Get<SceneAsset>(assetId); sceneAsset)
    return CreateNodePrefab(assetId, *sceneAsset);
  return EntityID::Invalid;
}
template <>
inline auto AssetManager::CreatePrefab<SkyboxAsset>(const AssetID assetId) -> EntityID {
  if (auto skyboxAsset = Get<SkyboxAsset>(assetId); skyboxAsset) {
    const auto name = GetName(assetId);
    const auto prefabId = prefabManager.Create(name);
    auto skyboxHandle = prefabManager.AddComponent<SkyboxHandle>(prefabId);
    skyboxHandle->assetId = assetId;
    return prefabId;
  }
  return EntityID::Invalid;
}
template <>
inline auto AssetManager::Load<SceneAsset>(const AssetID id, const std::filesystem::path &path, std::string name) -> std::unique_ptr<Asset> {
  Assimp::Importer importer;
  const auto aiScene = importer.ReadFile(path.string(), aiProcess_CalcTangentSpace | aiProcess_GlobalScale | aiProcess_JoinIdenticalVertices | aiProcess_SortByPType | aiProcess_Triangulate);
  if (!aiScene) {
    spdlog::error("Assimp: {}", importer.GetErrorString());
    return nullptr;
  }
  if (!aiScene->mRootNode)
    return nullptr;
  auto scene = std::make_unique<SceneAsset>(id, std::move(name));
  LoadNode(*aiScene->mRootNode, *aiScene, *scene.get());
  spdlog::info("Loaded scene asset: {}", path.string());
  return scene;
}
template <>
inline auto AssetManager::Load<ShaderAsset>(const AssetID id, const std::filesystem::path &path, std::string name) -> std::unique_ptr<Asset> {
  auto shader = std::make_unique<ShaderAsset>(id, std::move(name));
  std::ifstream fs(path);
  if (!fs) {
    spdlog::error("Failed to open shader file: {}", path.string());
    return shader;
  }
  std::stringstream ss;
  ss << fs.rdbuf();
  if (fs.fail()) {
    spdlog::error("Failed to read shader file: {}", path.string());
    return shader;
  }
  fs.close();
  shader->text = ss.str();
  spdlog::info("Loaded shader asset: {}", path.string());
  return shader;
}
template <>
inline auto AssetManager::Load<SkyboxAsset>(const AssetID id, const std::filesystem::path &path, std::string name) -> std::unique_ptr<Asset> {
  auto skybox = std::make_unique<SkyboxAsset>(id, std::move(name));
  const auto ext = path.extension().string();
  if (ext == ".exr") {
    float *data = nullptr;
    const char *errMsg = nullptr;
    auto result = LoadEXR(&data, &skybox->width, &skybox->height, path.string().c_str(), &errMsg);
    if (result != TINYEXR_SUCCESS) {
      if (errMsg) {
        spdlog::error("TinyEXR: {}", errMsg);
        FreeEXRErrorMessage(errMsg);
      } else
        spdlog::error("TinyEXR failed to load: {}", path.string());
    } else if (data) {
      skybox->channels = 4;
      const auto size = skybox->width * skybox->height * skybox->channels;
      skybox->data.assign(data, data + size);
      free(data);
      spdlog::info("Loaded skybox asset: {}", path.string());
    }
  } else if (auto data = stbi_loadf(path.string().c_str(), &skybox->width, &skybox->height, &skybox->channels, 0); data) {
    const auto size = skybox->width * skybox->height * skybox->channels;
    skybox->data.assign(data, data + size);
    stbi_image_free(data);
    spdlog::info("Loaded skybox asset: {}", path.string());
  }
  return skybox;
}