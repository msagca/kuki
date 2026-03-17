template <>
inline auto GLResourceManager::LoadAsset<SceneAsset>(SceneAsset &sceneAsset) -> void {
  // NOTE: materials and textures are loaded in the process if they are referenced by any mesh
  for (auto i = 0; i < sceneAsset.meshes.size(); ++i)
    LoadSceneMesh(sceneAsset, i);
}
template <>
inline auto GLResourceManager::LoadAsset<ShaderAsset>(ShaderAsset &compAsset) -> void {
  const auto &name = compAsset.GetName();
  if (resourceManager.IsEntity(name))
    return;
  const auto resourceId = resourceManager.Create(name);
  compAsset.resourceId = resourceId;
  auto compute = resourceManager.AddComponent<GLComputeShader>(resourceId);
  auto compId = GLShaderBase::Compile(compAsset.text.data(), GL_COMPUTE_SHADER);
  const auto programId = glCreateProgram();
  compute->id = programId;
  glAttachShader(programId, compId);
  glLinkProgram(programId);
  glDeleteShader(compId);
  int success;
  glGetProgramiv(programId, GL_LINK_STATUS, &success);
  if (success)
    compute->CacheLocations();
}
template <>
inline auto GLResourceManager::LoadAsset<ShaderAsset>(ShaderAsset &vertAsset, ShaderAsset &fragAsset) -> void {
  const auto &name = fragAsset.GetName();
  if (resourceManager.IsEntity(name))
    return;
  const auto resourceId = resourceManager.Create(name);
  vertAsset.resourceId = resourceId;
  fragAsset.resourceId = resourceId;
  GLShader *shader{};
  switch (fragAsset.type) {
  case MaterialType::Lit:
  case MaterialType::LitSkinned:
    shader = resourceManager.AddComponent<GLLitShader>(resourceId);
    break;
  case MaterialType::Unlit:
    shader = resourceManager.AddComponent<GLUnlitShader>(resourceId);
    break;
  default:
    shader = resourceManager.AddComponent<GLShader>(resourceId);
    break;
  }
  const auto vertId = GLShaderBase::Compile(vertAsset.text.data(), GL_VERTEX_SHADER);
  const auto fragId = GLShaderBase::Compile(fragAsset.text.data(), GL_FRAGMENT_SHADER);
  const auto programId = glCreateProgram();
  shader->id = programId;
  glAttachShader(programId, vertId);
  glAttachShader(programId, fragId);
  glLinkProgram(programId);
  glDeleteShader(vertId);
  glDeleteShader(fragId);
  int success;
  glGetProgramiv(programId, GL_LINK_STATUS, &success);
  if (success)
    shader->CacheLocations();
}
