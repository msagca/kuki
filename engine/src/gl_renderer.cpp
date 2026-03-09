#include <buffer_object.hpp>
#include <compute_type.hpp>
#include <gl_buffer.hpp>
#include <gl_compute_shader.hpp>
#include <gl_mesh_material.hpp>
#include <gl_render_target.hpp>
#include <gl_renderer.hpp>
#include <gl_resource_manager.hpp>
#include <gl_skybox.hpp>
#include <gl_texture.hpp>
#include <id.hpp>
#include <material_type.hpp>
#include <primitive.hpp>
#include <render_graph.hpp>
#include <render_target.hpp>
#include <scene.hpp>
#include <target_description.hpp>
//
#include <glad/glad.h>
namespace kuki {
auto GLRenderer::BorrowBuffer(const BufferDescription &desc) -> unsigned int {
  return resourceManager.BorrowBuffer(desc);
}
auto GLRenderer::BorrowFramebuffer() -> unsigned int {
  return resourceManager.BorrowFramebuffer();
}
auto GLRenderer::BorrowRenderbuffer(const TargetDescription &desc) -> unsigned int {
  return resourceManager.BorrowRenderbuffer(desc);
}
auto GLRenderer::BorrowTexture(const TargetDescription &desc) -> unsigned int {
  return resourceManager.BorrowTexture(desc);
}
auto GLRenderer::CreateBuffer(std::string name, const BufferDescription &desc) -> BufferObject * {
  const auto id = resourceManager.CreateBuffer(name, desc);
  return resourceManager.GetBuffer(id);
}
auto GLRenderer::CreateCompute(const ComputeType type, const ShaderAsset &compute) -> GLComputeShader * {
  const auto id = resourceManager.CreateCompute(type, compute);
  return resourceManager.GetCompute(type);
}
auto GLRenderer::CreatePrimitive(const PrimitiveType type) -> BufferObject * {
  const auto id = resourceManager.CreatePrimitive(type);
  return resourceManager.GetPrimitive(type);
}
auto GLRenderer::CreateShader(const MaterialType type, const ShaderAsset &vert, const ShaderAsset &frag) -> GLShader * {
  const auto id = resourceManager.CreateShader(type, vert, frag);
  return resourceManager.GetShader(type);
}
auto GLRenderer::CreateTarget(std::string name, const TargetDescription &desc) -> GLRenderTarget * {
  const auto id = resourceManager.CreateTarget(name, desc);
  return resourceManager.GetTarget(id);
}
auto GLRenderer::GetActiveScene() -> Scene * {
  return sceneManager.GetActive();
}
auto GLRenderer::GetCompute(const ComputeType type) -> GLComputeShader * {
  return resourceManager.GetCompute(type);
}
auto GLRenderer::GetPrimitive(const PrimitiveType type) -> GLMesh * {
  return resourceManager.GetPrimitive(type);
}
auto GLRenderer::GetScene(const std::string &name) -> Scene * {
  return sceneManager.Get(name);
}
auto GLRenderer::GetShader(const MaterialType type) -> GLShader * {
  return resourceManager.GetShader(type);
}
auto GLRenderer::GetTarget(const std::string &name) -> GLRenderTarget * {
  const auto id = resourceManager.GetID(name);
  return resourceManager.GetTarget(id);
}
auto GLRenderer::Reset() -> void {
  resourceManager.Clear();
}
auto GLRenderer::ApplyAntiAliasing(Renderer &renderer, std::span<std::string> inputs, std::span<TargetBinding> outputs) -> void {
  if (inputs.size() != 1 || outputs.size() != 1)
    return;
  const auto &desc = outputs[0].desc;
  const auto in = static_cast<GLRenderTarget *>(renderer.GetTarget(inputs[0]));
  const auto out = static_cast<GLRenderTarget *>(renderer.CreateTarget(outputs[0].name, outputs[0].desc));
  glBindFramebuffer(GL_FRAMEBUFFER, in->framebuffer);
  glViewport(0, 0, desc.width, desc.height);
  glClearColor(0.f, 0.f, 0.f, 0.f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
  glBindFramebuffer(GL_READ_FRAMEBUFFER, in->framebuffer);
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, out->framebuffer);
  glBlitFramebuffer(0, 0, desc.width, desc.height, 0, 0, desc.width, desc.height, GL_COLOR_BUFFER_BIT, GL_LINEAR);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
auto GLRenderer::ApplyBloomEffect(Renderer &renderer, std::span<std::string> inputs, std::span<TargetBinding> outputs) -> void {
  if (inputs.size() != 2 || outputs.size() != 1)
    return;
  auto bloomShader = static_cast<GLShader *>(renderer.GetShader(MaterialType::Bloom));
  if (!bloomShader)
    return;
  const auto mesh = static_cast<GLMesh *>(renderer.GetPrimitive(PrimitiveType::Frame));
  if (!mesh)
    return;
  const auto &desc = outputs[0].desc;
  const auto in0 = static_cast<GLRenderTarget *>(renderer.GetTarget(inputs[0]));
  const auto in1 = static_cast<GLRenderTarget *>(renderer.GetTarget(inputs[1]));
  const auto out = static_cast<GLRenderTarget *>(renderer.CreateTarget(outputs[0].name, outputs[0].desc));
  glBindFramebuffer(GL_FRAMEBUFFER, out->framebuffer);
  glViewport(0, 0, desc.width, desc.height);
  glClearColor(0.f, 0.f, 0.f, 0.f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  bloomShader->Use();
  bloomShader->SetTexture("image", in0->texture);
  bloomShader->SetTexture("imageBright", in1->texture);
  bloomShader->SetUniform("model", glm::mat4(1.f));
  bloomShader->Draw(*mesh);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
auto GLRenderer::ApplyBlurEffect(Renderer &renderer, std::span<std::string> inputs, std::span<TargetBinding> outputs) -> void {
  if (inputs.size() != 1 || outputs.size() != 1)
    return;
  auto blurShader = static_cast<GLShader *>(renderer.GetShader(MaterialType::Blur));
  if (!blurShader)
    return;
  const auto mesh = static_cast<GLMesh *>(renderer.GetPrimitive(PrimitiveType::Frame));
  if (!mesh)
    return;
  const auto &desc = outputs[0].desc;
  const auto in = static_cast<GLRenderTarget *>(renderer.GetTarget(inputs[0]));
  const auto out = static_cast<GLRenderTarget *>(renderer.CreateTarget(outputs[0].name, outputs[0].desc));
  constexpr auto blurPasses = 8;
  blurShader->Use();
  blurShader->SetUniform("model", glm::mat4(1.f));
  for (auto i = 0; i < blurPasses; ++i) {
    const auto pingPong = i % 2 == 0; // NOTE: this is `true` in the first iteration, so `in->texture` is read first (as it should be)
    const auto srcImg = pingPong ? in->texture : out->texture;
    const auto dstBuf = pingPong ? out->framebuffer : in->framebuffer;
    glBindFramebuffer(GL_FRAMEBUFFER, dstBuf);
    glViewport(0, 0, desc.width, desc.height);
    glClearColor(0.f, 0.f, 0.f, 0.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    blurShader->SetTexture("image", srcImg);
    blurShader->SetUniform("horizontal", pingPong);
    blurShader->Draw(*mesh);
  }
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
auto GLRenderer::ApplyBrightPassFilter(Renderer &renderer, std::span<std::string> inputs, std::span<TargetBinding> outputs) -> void {
  if (inputs.size() != 1 || outputs.size() != 1)
    return;
  auto brightShader = static_cast<GLShader *>(renderer.GetShader(MaterialType::BrightPass));
  if (!brightShader)
    return;
  const auto mesh = static_cast<GLMesh *>(renderer.GetPrimitive(PrimitiveType::Frame));
  if (!mesh)
    return;
  const auto &desc = outputs[0].desc;
  const auto in = static_cast<GLRenderTarget *>(renderer.GetTarget(inputs[0]));
  const auto out = static_cast<GLRenderTarget *>(renderer.CreateTarget(outputs[0].name, outputs[0].desc));
  glBindFramebuffer(GL_FRAMEBUFFER, out->framebuffer);
  glViewport(0, 0, desc.width, desc.height);
  glClearColor(0.f, 0.f, 0.f, 0.f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  brightShader->Use();
  brightShader->SetTexture("image", in->texture);
  brightShader->SetUniform("model", glm::mat4(1.f));
  brightShader->Draw(*mesh);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
auto GLRenderer::ApplyGammaCorrection(Renderer &renderer, std::span<std::string> inputs, std::span<TargetBinding> outputs) -> void {
  if (inputs.size() != 1 || outputs.size() != 1)
    return;
  auto gammaShader = static_cast<GLShader *>(renderer.GetShader(MaterialType::GammaCorrect));
  if (!gammaShader)
    return;
  const auto mesh = static_cast<GLMesh *>(renderer.GetPrimitive(PrimitiveType::Frame));
  if (!mesh)
    return;
  const auto &desc = outputs[0].desc;
  const auto in = static_cast<GLRenderTarget *>(renderer.GetTarget(inputs[0]));
  const auto out = static_cast<GLRenderTarget *>(renderer.CreateTarget(outputs[0].name, outputs[0].desc));
  glBindFramebuffer(GL_FRAMEBUFFER, out->framebuffer);
  glViewport(0, 0, desc.width, desc.height);
  glClearColor(0.f, 0.f, 0.f, 0.f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  gammaShader->Use();
  gammaShader->SetTexture("image", in->texture);
  // TODO: do not hardcode these values
  gammaShader->SetUniform("exposure", 1.f);
  gammaShader->SetUniform("gamma", 2.2f);
  gammaShader->SetUniform("model", glm::mat4(1.f));
  gammaShader->Draw(*mesh);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
auto GLRenderer::ConvertCubemapToEquirectangularMap(Renderer &renderer, const std::string &input, const TargetBinding &output) -> void {
  auto compute = static_cast<GLComputeShader *>(renderer.GetCompute(ComputeType::CubemapEquirect));
  if (!compute)
    return;
  const auto &desc = output.desc;
  const auto in = static_cast<GLRenderTarget *>(renderer.GetTarget(input));
  const auto out = static_cast<GLRenderTarget *>(renderer.CreateTarget(output.name, output.desc));
  constexpr unsigned int workgroupSize = 8;
  compute->Use();
  compute->SetTexture("cubemap", in->texture);
  compute->SetUniform("size", static_cast<unsigned int>(desc.width));
  const auto format = GLResourceManager::TargetFormatToGL(desc.format);
  glBindImageTexture(0, out->texture, 0, GL_TRUE, 0, GL_WRITE_ONLY, format);
  const auto numGroupsX = static_cast<unsigned int>(std::ceil(static_cast<float>(desc.width) / workgroupSize));
  const auto numGroupsY = static_cast<unsigned int>(std::ceil(static_cast<float>(desc.height) / workgroupSize));
  compute->Dispatch(numGroupsX, numGroupsY, 6);
}
auto GLRenderer::ConvertEquirectangularMapToCubemap(Renderer &renderer, const std::string &input, const TargetBinding &output) -> void {
  auto compute = static_cast<GLComputeShader *>(renderer.GetCompute(ComputeType::EquirectCubemap));
  if (!compute)
    return;
  const auto &desc = output.desc;
  const auto in = static_cast<GLRenderTarget *>(renderer.GetTarget(input));
  const auto out = static_cast<GLRenderTarget *>(renderer.CreateTarget(output.name, output.desc));
  const auto format = GLResourceManager::TargetFormatToGL(desc.format);
  constexpr unsigned int workgroupSize = 8;
  compute->Use();
  compute->SetTexture("equirect", in->texture);
  compute->SetUniform("size", static_cast<unsigned int>(desc.width));
  // TODO: set the following to `true` if texture was loaded by TinyEXR
  compute->SetUniform("invert", false);
  glBindImageTexture(0, out->texture, 0, GL_TRUE, 0, GL_WRITE_ONLY, format);
  const auto numGroupsX = static_cast<unsigned int>(std::ceil(static_cast<float>(desc.width) / workgroupSize));
  const auto numGroupsY = static_cast<unsigned int>(std::ceil(static_cast<float>(desc.height) / workgroupSize));
  compute->Dispatch(numGroupsX, numGroupsY, 6);
}
auto GLRenderer::CreateBRDF_LUT(Renderer &renderer, const TargetBinding &output) -> void {
  auto compute = static_cast<GLComputeShader *>(renderer.GetCompute(ComputeType::BRDF_LUT));
  if (!compute)
    return;
  const auto &desc = output.desc;
  const auto out = static_cast<GLRenderTarget *>(renderer.CreateTarget(output.name, output.desc));
  const auto format = GLResourceManager::TargetFormatToGL(desc.format);
  constexpr unsigned int workgroupSize = 8;
  compute->Use();
  glBindImageTexture(0, out->texture, 0, GL_FALSE, 0, GL_WRITE_ONLY, format);
  const auto numGroupsX = static_cast<unsigned int>(std::ceil(static_cast<float>(desc.width) / workgroupSize));
  const auto numGroupsY = static_cast<unsigned int>(std::ceil(static_cast<float>(desc.height) / workgroupSize));
  compute->Dispatch(numGroupsX, numGroupsY, 1);
}
auto GLRenderer::CreateIrradianceMap(Renderer &renderer, const std::string &input, const TargetBinding &output) -> void {
  auto compute = static_cast<GLComputeShader *>(renderer.GetCompute(ComputeType::IrradianceMap));
  if (!compute)
    return;
  const auto &desc = output.desc;
  const auto in = static_cast<GLRenderTarget *>(renderer.GetTarget(input));
  const auto out = static_cast<GLRenderTarget *>(renderer.CreateTarget(output.name, output.desc));
  const auto format = GLResourceManager::TargetFormatToGL(desc.format);
  constexpr unsigned int workgroupSize = 8;
  compute->Use();
  compute->SetTexture("cubemap", in->texture);
  compute->SetUniform("cubeSize", static_cast<unsigned int>(desc.width));
  glBindImageTexture(0, out->texture, 0, GL_TRUE, 0, GL_WRITE_ONLY, format);
  const auto numGroupsX = static_cast<unsigned int>(std::ceil(static_cast<float>(desc.width) / workgroupSize));
  const auto numGroupsY = static_cast<unsigned int>(std::ceil(static_cast<float>(desc.height) / workgroupSize));
  compute->Dispatch(numGroupsX, numGroupsY, 6);
}
auto GLRenderer::CreatePrefilterMap(Renderer &renderer, const std::string &input, const TargetBinding &output) -> void {
  auto compute = static_cast<GLComputeShader *>(renderer.GetCompute(ComputeType::PrefilterMap));
  if (!compute)
    return;
  const auto &desc = output.desc;
  const auto in = static_cast<GLRenderTarget *>(renderer.GetTarget(input));
  const auto out = static_cast<GLRenderTarget *>(renderer.CreateTarget(output.name, output.desc));
  const auto format = GLResourceManager::TargetFormatToGL(desc.format);
  constexpr unsigned int workgroupSize = 8;
  compute->Use();
  compute->SetTexture("cubemap", in->texture);
  compute->SetTexture("mipLevels", desc.mipmaps);
  for (auto mip = 0; mip < desc.mipmaps; ++mip) {
    const auto mipSize = static_cast<unsigned int>(desc.width) >> mip;
    const auto roughness = static_cast<float>(mip) / (desc.mipmaps - 1);
    compute->SetUniform("roughness", roughness);
    compute->SetUniform("mipWidth", mipSize);
    compute->SetUniform("cubeSize", mipSize);
    glBindImageTexture(0, out->texture, mip, GL_TRUE, 0, GL_WRITE_ONLY, format);
    const auto numGroupsX = static_cast<unsigned int>(std::ceil(static_cast<float>(desc.width) / workgroupSize));
    const auto numGroupsY = static_cast<unsigned int>(std::ceil(static_cast<float>(desc.height) / workgroupSize));
    compute->Dispatch(numGroupsX, numGroupsY, 6);
  }
}
auto GLRenderer::RenderScene(Renderer &renderer, std::span<std::string> inputs, std::span<TargetBinding> outputs) -> void {
  if (outputs.size() != 1)
    return;
  auto scene = renderer.GetActiveScene();
  if (!scene)
    return;
  const auto &desc = outputs[0].desc;
  const auto out = static_cast<GLRenderTarget *>(renderer.CreateTarget(outputs[0].name, outputs[0].desc));
  glBindFramebuffer(GL_FRAMEBUFFER, out->framebuffer);
  glGenerateMipmap(GL_TEXTURE_2D);
  glViewport(0, 0, desc.width, desc.height);
  glClearColor(0.f, 0.f, 0.f, 0.f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
  DrawSkybox(renderer);
  DrawEntities(renderer);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
auto GLRenderer::DrawEntities(Renderer &renderer) -> void {
  auto scene = renderer.GetActiveScene();
  if (!scene)
    return;
  // TODO: cache these
  std::unordered_map<GLMeshMat, std::vector<glm::mat4>> meshToMatToTransforms;
  std::unordered_map<GLMeshMat, std::vector<MaterialFallback>> meshToMatToFallbacks;
  // TODO: use ForEachVisibleEntity below
  scene->ForEachEntity<GLMesh, GLMaterial, Transform>([&meshToMatToTransforms, &meshToMatToFallbacks](const EntityID id, const GLMesh *mesh, const GLMaterial *material, const Transform *transform) {
    if (mesh->vao == 0)
      return;
    GLMeshMat meshMat{.mesh = *mesh, .material = *material};
    meshToMatToTransforms[meshMat].push_back(transform->world);
    meshToMatToFallbacks[meshMat].push_back(material->fallback);
  });
  for (const auto &[meshMat, transforms] : meshToMatToTransforms)
    DrawEntitiesInstanced(renderer, meshMat.mesh, meshMat.material, meshToMatToFallbacks[meshMat], transforms);
}
auto GLRenderer::DrawEntitiesInstanced(Renderer &renderer, const GLMesh &mesh, const GLMaterial &material, const std::vector<MaterialFallback> &fallbacks, const std::vector<glm::mat4> &transforms) -> void {
  auto scene = renderer.GetActiveScene();
  if (!scene)
    return;
  auto camera = scene->GetActiveCamera();
  if (!camera)
    return;
  auto shader = static_cast<GLShader *>(renderer.GetShader(material.type));
  if (!shader)
    return;
  const auto bufMat = static_cast<GLBuffer *>(renderer.CreateBuffer("MaterialBuffer", {}));
  const auto bufXfm = static_cast<GLBuffer *>(renderer.CreateBuffer("TransformBuffer", {}));
  const auto bufDesc = BufferDescription{.size = sizeof(CameraTransform)};
  const auto camBuf = static_cast<GLBuffer *>(renderer.CreateBuffer("CameraBuffer", bufDesc));
  shader->Use();
  shader->SetCamera(*camera, camBuf->id);
  if (material.type == MaterialType::Lit) {
    GLSkybox *skybox{nullptr};
    scene->ForFirstEntity<GLSkybox>([&](const EntityID, const GLSkybox *skyboxComp) {
      *skybox = *skyboxComp;
    });
    shader->SetSkybox(skybox);
    std::vector<Light> lights;
    scene->ForEachEntity<Light>([&](EntityID, const Light *light) {
      lights.push_back(*light);
    });
    shader->SetLighting(lights);
  }
  shader->SetMaterial(material);
  shader->SetMaterialFallback(mesh, fallbacks, bufMat->id);
  shader->SetTransform(mesh, transforms, bufXfm->id);
  shader->DrawInstanced(mesh, transforms.size());
}
auto GLRenderer::DrawSkybox(Renderer &renderer) -> void {
  auto scene = renderer.GetActiveScene();
  if (!scene)
    return;
  auto camera = scene->GetActiveCamera();
  if (!camera)
    return;
  auto shader = static_cast<GLShader *>(renderer.GetShader(MaterialType::Skybox));
  if (!shader)
    return;
  const auto mesh = static_cast<GLMesh *>(renderer.GetPrimitive(PrimitiveType::CubeInverted));
  if (!mesh)
    return;
  const auto bufDesc = BufferDescription{.size = sizeof(CameraTransform)};
  const auto camBuf = static_cast<GLBuffer *>(renderer.CreateBuffer("CameraBuffer", bufDesc));
  shader->Use();
  shader->SetCamera(*camera, camBuf->id);
  const auto model = glm::scale(glm::mat4(1.f), glm::vec3(2.f));
  shader->SetUniform("model", model);
  GLSkybox *skybox = nullptr;
  scene->ForFirstEntity<GLSkybox>([&skybox](const EntityID id, GLSkybox *skyboxComp) {
    skybox = skyboxComp;
  });
  if (!skybox || skybox->skybox == 0)
    shader->SetUniform("useSkybox", false);
  else {
    shader->SetTexture("skybox", skybox->skybox);
    shader->SetUniform("useSkybox", true);
  }
  glDepthFunc(GL_LEQUAL);
  shader->Draw(*mesh);
  glDepthFunc(GL_LESS);
}
} // namespace kuki
